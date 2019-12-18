/*
 * FPGA DMA transfer module
 *
 * Copyright Altera Corporation (C) 2014. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#define DEBUG
#include <linux/cdev.h>
#include <linux/scatterlist.h>
#include <linux/completion.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/pm.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/uaccess.h>

/****************************************************************************/

static unsigned int max_burst_words = 16;
module_param(max_burst_words, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(max_burst_words, "Size of a burst in words "
		 "(in this case a word is 64 bits)");

static int timeout = 1000;
module_param(timeout, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(timeout, "Transfer Timeout in msec (default: 1000), "
		 "Pass -1 for infinite timeout");

#define DMA_MAP_SG_DUMB /*DUMB!*/

#ifdef DMA_MAP_SG_DUMB
#define dma_drv_map_sg		dma_map_sg_dumb
#define dma_drv_unmap_sg	dma_unmap_sg_dumb
#else
#define dma_drv_map_sg		dma_map_sg_attrs
#define dma_drv_unmap_sg	dma_unmap_sg
#endif

#define ALT_FPGADMA_DATA_WRITE		0x00
#define ALT_FPGADMA_DATA_READ		0x08

#define ALT_FPGADMA_CSR_WR_WTRMK	0x00
#define ALT_FPGADMA_CSR_RD_WTRMK	0x04
#define ALT_FPGADMA_CSR_BURST		0x08
#define ALT_FPGADMA_CSR_FIFO_STATUS	0x0C
#define ALT_FPGADMA_CSR_DATA_WIDTH	0x10
#define ALT_FPGADMA_CSR_FIFO_DEPTH	0x14
#define ALT_FPGADMA_CSR_FIFO_CLEAR	0x18
#define ALT_FPGADMA_CSR_ZERO		0x1C

#define ALT_FPGADMA_CSR_BURST_TX_SINGLE	(1 << 0)
#define ALT_FPGADMA_CSR_BURST_TX_BURST	(1 << 1)
#define ALT_FPGADMA_CSR_BURST_RX_SINGLE	(1 << 2)
#define ALT_FPGADMA_CSR_BURST_RX_BURST	(1 << 3)

#define ALT_FPGADMA_FIFO_FULL		(1 << 25)
#define ALT_FPGADMA_FIFO_EMPTY		(1 << 24)
#define ALT_FPGADMA_FIFO_USED_MASK	((1 << 24)-1)

struct fpga_dma_pdata {

	struct platform_device *pdev;

	struct dentry *root;

	unsigned int data_reg_phy;
	void __iomem *data_reg;
	void __iomem *csr_reg;

	unsigned int fifo_size_bytes;
	unsigned int fifo_depth;
	unsigned int data_width;
	unsigned int data_width_bytes;
	unsigned char *read_buf;
	unsigned char *write_buf;

	struct dma_chan *txchan;
	struct dma_chan *rxchan;
	dma_addr_t tx_dma_addr;
	dma_addr_t rx_dma_addr;
	dma_cookie_t rx_cookie;
	dma_cookie_t tx_cookie;
};

static DECLARE_COMPLETION(dma_read_complete);
static DECLARE_COMPLETION(dma_write_complete);

#define IS_DMA_READ (true)
#define IS_DMA_WRITE (false)

static int fpga_dma_dma_start_rx(struct platform_device *pdev,
				 unsigned datalen, const char __user *databuf,
				 u32 burst_size);
static int fpga_dma_dma_start_tx(struct platform_device *pdev,
				 unsigned datalen, const char __user *databuf,
				 u32 burst_size);
typedef struct {
	void __user *vaddr;
	void *kaddr;
	dma_addr_t daddr;
	size_t len;
	size_t off1st;
	size_t llast;
	size_t pgnum;
	size_t sgnum;
	struct page **pages;
	struct scatterlist *sgs;
	enum dma_data_direction dir;
} usrbuf_t;
/* --------------------------------------------------------------------- */

static size_t
/*
*Function to caculate the page numer for scatter-list
*/ 
calc_pgs_num(usrbuf_t *usrbuf)
{
	size_t sz1st;
	if (usrbuf->len) {
		usrbuf->pgnum = 1;
	} else {
		return 0;
	}
	usrbuf->off1st = (size_t)(usrbuf->vaddr) & (PAGE_SIZE - 1);
	sz1st = PAGE_SIZE - usrbuf->off1st;
	if (sz1st > usrbuf->len) {
		usrbuf->llast = 0;
	} else {
		usrbuf->pgnum += (usrbuf->len - sz1st) / PAGE_SIZE;
		usrbuf->llast = (usrbuf->len - sz1st) & (PAGE_SIZE - 1);
		if (usrbuf->llast)
			++usrbuf->pgnum;
	}
	return usrbuf->pgnum;
}
static void 
populate_sgs(usrbuf_t *usrbuf)
{
/*
*Function to populate the page numer for scatter-list
*/ 
	size_t i, len = usrbuf->len;
	size_t sglen = min((size_t)(PAGE_SIZE - usrbuf->off1st), len);

	sg_init_table(usrbuf->sgs, usrbuf->pgnum);

	/* 1st page definitely has nonzero off */
	sg_set_page(usrbuf->sgs, usrbuf->pages[0], sglen, usrbuf->off1st);
	len -= sglen;
	/* iterate remaining sg */
	for (i = 1; i < usrbuf->pgnum; i++) {
		sglen = min((size_t)PAGE_SIZE, len);
		sg_set_page(usrbuf->sgs + i, usrbuf->pages[i], sglen, 0);
		len -= sglen;
	}
}
static int dma_map_sg_dumb(struct device *dev, struct scatterlist *sg,
		int nents, enum dma_data_direction dir,
		unsigned long attrs)
{
/*
*Mapping dma using loop 
*/
	int i;
	for (i = 0; i != nents; i++) {
		sg[i].dma_address = dma_map_page(dev,
						 (struct page *)sg[i].page_link,
						 sg[i].offset,
						 sg[i].length, dir);
		if (dma_mapping_error(dev, sg[i].dma_address)) {
			goto ROLL_BACK;
		}
	}
	return nents;
ROLL_BACK:
	printk(KERN_ERR
	       "dma_map_sg_dumb() fail, i=%d, &p=%px, o=%d, l=%d\n",
	       i, (struct page *)sg[i].page_link, sg[i].offset,
	       sg[i].length);
	if (--i > 0) {
		for (; i >= 0; i--) {
			dma_unmap_page(dev,
				       sg[i].dma_address, sg[i].length, dir);
		}
	}
	return 0;
}
static void dma_unmap_sg_dumb(struct device *dev, struct scatterlist *sg,
		  int nents, enum dma_data_direction dir)
{
/*
*Unmapping dma using loop 
*/
	int i;
	for (i = 0; i != nents; i++) {
		dma_unmap_page(dev, sg[i].dma_address, sg[i].length, dir);
	}
}

static usrbuf_t *get_usr_buf(struct platform_device *dma_dev,
			     const char __user * buf, size_t len,
			     enum dma_data_direction dir)
{
	size_t pgnum;
	size_t i;
	struct device *dev = &dma_dev->dev;
/* ALLOC_USR_BUF */
	usrbuf_t *usrbuf = kmalloc(sizeof(usrbuf_t), GFP_KERNEL);
	if (NULL == usrbuf) {
		printk(KERN_ERR "kmalloc() usrbuf_t error!\n");
		return NULL;
	}
	// calculate number of pages in user buf
	usrbuf->vaddr = buf;
	usrbuf->len = len;
	usrbuf->dir = dir;
	pgnum = calc_pgs_num(usrbuf);
	printk("pgnum:%d\n",pgnum);
/* ALLOC PAGES */
	usrbuf->pages = kmalloc(pgnum * sizeof(struct page *), GFP_KERNEL);
	if (NULL == usrbuf->pages) {
		dev_err(dev, "kmalloc() pages error!\n");
		goto FREE_USR_BUF;
	}

/* SEM DOWN */
	down_read(&current->mm->mmap_sem);

/* GET PAGES */
	pgnum = get_user_pages((unsigned long)buf,
				pgnum,
				FOLL_WRITE,
				usrbuf->pages,
				NULL);
	printk("pgnum:%d\n",pgnum);
	if (!pgnum) {
	        dev_err(dev, "get_user_pages() error!\n");
		goto SEM_UP;
	}

	usrbuf->pgnum = pgnum;

/* ALLOC SGS */
	usrbuf->sgs = kmalloc(pgnum * sizeof(struct scatterlist), GFP_KERNEL);
	if (NULL == usrbuf->sgs) {
	        dev_err(dev, "kmalloc() sgs error!\n");
		goto PUT_PAGES;
	}

	populate_sgs(usrbuf);

/* DMA MAP SG */
	usrbuf->sgnum = dma_drv_map_sg(&dma_dev->dev,
				       usrbuf->sgs,
				       usrbuf->pgnum,
				       usrbuf->dir,
				       0);

	if (usrbuf->sgnum == 0) {
	        dev_err(dev, "dma_map_sg() error!\n");
		goto FREE_SGS;
	}
	up_read(&current->mm->mmap_sem);
	return usrbuf;

FREE_SGS:			/* !ALLOC SGS */
	kfree(usrbuf->sgs);
PUT_PAGES:			/* !GET PAGES */
	for (i = 0; i < usrbuf->pgnum; ++i)
		put_page(usrbuf->pages[i]);
SEM_UP:			/* !SEM DOWN */
	up_read(&current->mm->mmap_sem);
/* FREE_PAGES:				!ALLOC PAGES */
	kfree(usrbuf->pages);
FREE_USR_BUF:			/* !ALLOC_USR_BUF */
	kfree(usrbuf);
	return 0;
}

static void put_usr_buf(struct platform_device *dma_dev, usrbuf_t * usrbuf)
{
	size_t i;
/* UNMAP_SG:					 !DMA MAP SG */
	dma_drv_unmap_sg(&dma_dev->dev,
			 usrbuf->sgs,
			 usrbuf->sgnum,
			 usrbuf->dir);
/* FREE_SGS:					 !ALLOC SGS */
	kfree(usrbuf->sgs);
/* PUT_PAGES:				   !GET PAGES */
	for (i = 0; i < usrbuf->pgnum; ++i)
		put_page(usrbuf->pages[i]);
/* FREE_PAGES:				!ALLOC PAGES */
	kfree(usrbuf->pages);
/* FREE_USR_BUF:			!ALLOC_USR_BUF */
	kfree(usrbuf);
}


static void dump_csr(struct fpga_dma_pdata *pdata)
{
	dev_info(&pdata->pdev->dev, "ALT_FPGADMA_CSR_WR_WTRMK      %08x\n",
		 readl(pdata->csr_reg + ALT_FPGADMA_CSR_WR_WTRMK));
	dev_info(&pdata->pdev->dev, "ALT_FPGADMA_CSR_RD_WTRMK      %08x\n",
		 readl(pdata->csr_reg + ALT_FPGADMA_CSR_RD_WTRMK));
	dev_info(&pdata->pdev->dev, "ALT_FPGADMA_CSR_BURST         %08x\n",
		 readl(pdata->csr_reg + ALT_FPGADMA_CSR_BURST));
	dev_info(&pdata->pdev->dev, "ALT_FPGADMA_CSR_FIFO_STATUS   %08x\n",
		 readl(pdata->csr_reg + ALT_FPGADMA_CSR_FIFO_STATUS));
	dev_info(&pdata->pdev->dev, "ALT_FPGADMA_CSR_DATA_WIDTH    %08x\n",
		 readl(pdata->csr_reg + ALT_FPGADMA_CSR_DATA_WIDTH));
	dev_info(&pdata->pdev->dev, "ALT_FPGADMA_CSR_FIFO_DEPTH    %08x\n",
		 readl(pdata->csr_reg + ALT_FPGADMA_CSR_FIFO_DEPTH));
	dev_info(&pdata->pdev->dev, "ALT_FPGADMA_CSR_ZERO          %08x\n",
		 readl(pdata->csr_reg + ALT_FPGADMA_CSR_ZERO));
}

/* --------------------------------------------------------------------- */

static void recalc_burst_and_words(struct fpga_dma_pdata *pdata,
				   int *burst_size, int *num_words)
{
	/* adjust size and maxburst so that total bytes transferred
	   is a multiple of burst length and width */
	if (*num_words < max_burst_words) {
		/* we have only a few words left, make it our burst size */
		*burst_size = *num_words;
	} else {
		/* here we may not transfer all words to FIFO, but next
		   call will pick them up... */
		*num_words = max_burst_words * (*num_words / max_burst_words);
		*burst_size = max_burst_words;
	}
}

static int word_to_bytes(struct fpga_dma_pdata *pdata, int num_bytes)
{
	return (num_bytes + pdata->data_width_bytes - 1)
	    / pdata->data_width_bytes;
}

static ssize_t dbgfs_write_dma(struct file *file, const char __user *user_buf,
			       size_t count, loff_t *ppos)
{
	struct fpga_dma_pdata *pdata = file->private_data;
	int ret = 0;
	int bytes_to_transfer;
	int num_words;
	u32 burst_size;
	int pad_index;

	//*ppos = 0;
	printk("count:%d\n",count);
	/* get user data into kernel buffer */
	/*
	bytes_to_transfer = simple_write_to_buffer(pdata->write_buf,
						   pdata->fifo_size_bytes, ppos,
						   user_buf, count);
	*/
	bytes_to_transfer = count;
	pad_index = bytes_to_transfer;
	printk("bytes to transfer %d\n", bytes_to_transfer);
	num_words = word_to_bytes(pdata, bytes_to_transfer);
	recalc_burst_and_words(pdata, &burst_size, &num_words);
	/* we sometimes send more than asked for, padded with zeros */
	bytes_to_transfer = num_words * pdata->data_width_bytes;
	printk("bytes to transfer %d\n", bytes_to_transfer);
	for (; pad_index < bytes_to_transfer; pad_index++)
		pdata->write_buf[pad_index] = 0;

	ret = fpga_dma_dma_start_tx(pdata->pdev,
				    bytes_to_transfer, user_buf,
				    burst_size);
	if (ret) {
		dev_err(&pdata->pdev->dev, "Error starting TX DMA %d\n", ret);
		return ret;
	}

	if (!wait_for_completion_timeout(&dma_write_complete,
					 msecs_to_jiffies(timeout))) {
		dev_err(&pdata->pdev->dev, "Timeout waiting for TX DMA!\n");
		dev_err(&pdata->pdev->dev,
			"count %d burst_size %d num_words %d bytes_to_transfer %d\n",
			count, burst_size, num_words, bytes_to_transfer);
		dmaengine_terminate_all(pdata->txchan);
		return -ETIMEDOUT;
	}

	return bytes_to_transfer;
}

static ssize_t dbgfs_read_dma(struct file *file, char __user *user_buf,
			      size_t count, loff_t *ppos)
{
	struct fpga_dma_pdata *pdata = file->private_data;
	int ret;
	int num_words;
	int num_bytes;
	u32 burst_size;
	num_bytes = count;
	if (num_bytes > 0) {
		ret = fpga_dma_dma_start_rx(pdata->pdev, num_bytes,
					    user_buf, burst_size);
		if (ret) {
			dev_err(&pdata->pdev->dev,
				"Error starting RX DMA %d\n", ret);
			return ret;
		}

		if (!wait_for_completion_timeout(&dma_read_complete,
						 msecs_to_jiffies(timeout))) {
			dev_err(&pdata->pdev->dev,
				"Timeout waiting for RX DMA!\n");
			dmaengine_terminate_all(pdata->rxchan);
			return -ETIMEDOUT;
		}
		*ppos = 0;
	}
	return 0;
}

static const struct file_operations dbgfs_dma_fops = {
	.write = dbgfs_write_dma,
	.read = dbgfs_read_dma,
	.open = simple_open,
	.llseek = no_llseek,
};

/* --------------------------------------------------------------------- */

static ssize_t dbgfs_read_csr(struct file *file, char __user *user_buf,
			      size_t count, loff_t *ppos)
{
	struct fpga_dma_pdata *pdata = file->private_data;
	dump_csr(pdata);
	return 0;
}

static const struct file_operations dbgfs_csr_fops = {
	.read = dbgfs_read_csr,
	.open = simple_open,
	.llseek = no_llseek,
};

/* --------------------------------------------------------------------- */

static ssize_t dbgfs_write_clear(struct file *file,
				 const char __user *user_buf, size_t count,
				 loff_t *ppos)
{
	struct fpga_dma_pdata *pdata = file->private_data;
	writel(1, pdata->csr_reg + ALT_FPGADMA_CSR_FIFO_CLEAR);
	return count;
}

static const struct file_operations dbgfs_clear_fops = {
	.write = dbgfs_write_clear,
	.open = simple_open,
	.llseek = no_llseek,
};

/* --------------------------------------------------------------------- */

static ssize_t dbgfs_write_wrwtrmk(struct file *file,
				   const char __user *user_buf,
				   size_t count, loff_t *ppos)
{
	struct fpga_dma_pdata *pdata = file->private_data;
	char buf[32];
	unsigned long val;
	int ret;

	memset(buf, 0, sizeof(buf));

	if (copy_from_user(buf, user_buf, min(count, (sizeof(buf) - 1))))
		return -EFAULT;

	ret = kstrtoul(buf, 16, &val);
	if (ret)
		return ret;

	writel(val, pdata->csr_reg + ALT_FPGADMA_CSR_WR_WTRMK);
	return count;
}

static const struct file_operations dbgfs_wrwtrmk_fops = {
	.write = dbgfs_write_wrwtrmk,
	.open = simple_open,
	.llseek = no_llseek,
};

/* --------------------------------------------------------------------- */

static ssize_t dbgfs_write_rdwtrmk(struct file *file,
				   const char __user *user_buf,
				   size_t count, loff_t *ppos)
{
	struct fpga_dma_pdata *pdata = file->private_data;
	char buf[32];
	int ret;
	unsigned long val;

	memset(buf, 0, sizeof(buf));

	if (copy_from_user(buf, user_buf, min(count, (sizeof(buf) - 1))))
		return -EFAULT;

	ret = kstrtoul(buf, 16, &val);
	if (ret)
		return ret;

	writel(val, pdata->csr_reg + ALT_FPGADMA_CSR_RD_WTRMK);
	return count;
}

static const struct file_operations dbgfs_rdwtrmk_fops = {
	.write = dbgfs_write_rdwtrmk,
	.open = simple_open,
	.llseek = no_llseek,
};

static ssize_t dbgfs_fifo_write(struct file *file, const char __user *user_buf, 
	size_t count, loff_t *ppos){
	struct fpga_dma_pdata *pdata = file->private_data;
	char buf[32];
	unsigned long val;
	int ret;

	memset(buf, 0, sizeof(buf));

	if (copy_from_user(buf, user_buf, min(count, (sizeof(buf) - 1))))
		return -EFAULT;

	ret = kstrtoul(buf, 16, &val);
	if (ret)
		return ret;

	writel(val, pdata->csr_reg + ALT_FPGADMA_CSR_ZERO);
	return count;
}
/*
static ssize_t dbgfs_fifo_read(struct file *file, char __user *user_buf,
	size_t count, loff_t *ppos){
	return 0;
}
*/
static const struct file_operations dbgfs_fifo_fops = {
	.write = dbgfs_fifo_write,
	//.read = dbgfs_fifo_read,
	.open = simple_open,
	.llseek = no_llseek,
};
/* --------------------------------------------------------------------- */

static int fpga_dma_register_dbgfs(struct fpga_dma_pdata *pdata)
{
	struct dentry *d;

	d = debugfs_create_dir("fpga_dma", NULL);
	if (IS_ERR(d))
		return PTR_ERR(d);
	if (!d) {
		dev_err(&pdata->pdev->dev, "Failed to initialize debugfs\n");
		return -ENOMEM;
	}

	pdata->root = d;

	debugfs_create_file("dma", S_IWUSR | S_IRUGO, pdata->root, pdata,
			    &dbgfs_dma_fops);

	debugfs_create_file("csr", S_IRUGO, pdata->root, pdata,
			    &dbgfs_csr_fops);

	debugfs_create_file("clear", S_IWUSR, pdata->root, pdata,
			    &dbgfs_clear_fops);

	debugfs_create_file("wrwtrmk", S_IWUSR, pdata->root, pdata,
			    &dbgfs_wrwtrmk_fops);

	debugfs_create_file("rdwtrmk", S_IWUSR, pdata->root, pdata,
			    &dbgfs_rdwtrmk_fops);

	debugfs_create_file("fifo", S_IWUSR, pdata->root, pdata,
			     &dbgfs_fifo_fops);

	return 0;
}

/* --------------------------------------------------------------------- */

static void fpga_dma_dma_rx_done(void *arg)
{	
	printk("rx done\n");
	complete(&dma_read_complete);
}

static void fpga_dma_dma_tx_done(void *arg)
{
	printk("tx done\n");
	complete(&dma_write_complete);
}

static void fpga_dma_dma_cleanup(struct platform_device *pdev,
				 unsigned datalen, bool do_read)
{
	struct fpga_dma_pdata *pdata = platform_get_drvdata(pdev);
	if (do_read)
		dma_unmap_single(&pdev->dev, pdata->rx_dma_addr,
				 datalen, DMA_FROM_DEVICE);
	else
		dma_unmap_single(&pdev->dev, pdata->tx_dma_addr,
				 datalen, DMA_TO_DEVICE);
}

static int fpga_dma_dma_start_rx(struct platform_device *pdev,
				 unsigned datalen, const char __user *databuf,
				 u32 burst_size)
{
	struct fpga_dma_pdata *pdata = platform_get_drvdata(pdev);
	struct dma_chan *dmachan;
	struct dma_slave_config dmaconf;
	struct dma_async_tx_descriptor *dmadesc = NULL;

	int num_words;

	num_words = word_to_bytes(pdata, datalen);

	dmachan = pdata->rxchan;
	memset(&dmaconf, 0, sizeof(dmaconf));
	dmaconf.direction = DMA_DEV_TO_MEM;
	dmaconf.src_addr = pdata->data_reg_phy + ALT_FPGADMA_DATA_READ;
	dmaconf.src_addr_width = 8;
	dmaconf.src_maxburst = burst_size;
        usrbuf_t *usrbuf;
        usrbuf = get_usr_buf(pdev,
			     databuf,
			     datalen,
			     DMA_FROM_DEVICE);
	if (!usrbuf) {
		dev_err(&pdev->dev, "get_usr_buf() error!");
		return -ENOENT;
        } 
	/* set up slave config */
	if(dmaengine_slave_config(dmachan, &dmaconf)<0){
            dev_err(&pdev->dev, "dmaengine_slave_config() rx failure");
            put_usr_buf(pdev,usrbuf);
        }
    
	dmadesc = dmaengine_prep_slave_sg(dmachan,
					      usrbuf->sgs,
					      usrbuf -> sgnum,
					      dmaconf.direction,
					      DMA_PREP_INTERRUPT);
	if (!dmadesc) {
		fpga_dma_dma_cleanup(pdev, datalen, IS_DMA_READ);
                put_usr_buf(pdev,usrbuf);
		return -ENOMEM;
	}
	dmadesc->callback = fpga_dma_dma_rx_done;
	dmadesc->callback_param = pdata;

	/* start DMA */
	pdata->rx_cookie = dmaengine_submit(dmadesc);
	if (dma_submit_error(pdata->rx_cookie))
		dev_err(&pdev->dev, "rx_cookie error on dmaengine_submit\n");
	dma_async_issue_pending(dmachan);

	return 0;
}

static int fpga_dma_dma_start_tx(struct platform_device *pdev,
				 unsigned datalen, const char __user *databuf,
				 u32 burst_size)
{
	struct fpga_dma_pdata *pdata = platform_get_drvdata(pdev);
	struct dma_chan *dmachan;
	struct dma_slave_config dmaconf;
	struct dma_async_tx_descriptor *dmadesc = NULL;

	int num_words;

	num_words = word_to_bytes(pdata, datalen);

	dmachan = pdata->txchan;
	memset(&dmaconf, 0, sizeof(dmaconf));
	dmaconf.direction = DMA_MEM_TO_DEV;
	dmaconf.dst_addr = pdata->data_reg_phy + ALT_FPGADMA_DATA_WRITE;
	dmaconf.dst_addr_width = 8;
	dmaconf.dst_maxburst = burst_size;
 
        usrbuf_t *usrbuf;
        usrbuf = get_usr_buf(pdev,
                             databuf,
                             datalen,
                             DMA_TO_DEVICE);
        if (!usrbuf) {
                dev_err(&pdev->dev, "get_usr_buf() error!");
                return -ENOENT;
        }
  
        
	/* set up slave config */
	if(dmaengine_slave_config(dmachan, &dmaconf)<0){
            dev_err(&pdev->dev, "dmaengine_slave_config() tx failure");
            put_usr_buf(pdev,usrbuf);
        }


	/* get dmadesc */
	dmadesc = dmaengine_prep_slave_sg(dmachan,
					      usrbuf->sgs,
					      usrbuf->sgnum,
					      dmaconf.direction,
					      DMA_PREP_INTERRUPT);
	if (!dmadesc) {
		fpga_dma_dma_cleanup(pdev, datalen, IS_DMA_WRITE);
                put_usr_buf(pdev,usrbuf);
		return -ENOMEM;
	}
	dmadesc->callback = fpga_dma_dma_tx_done;
	dmadesc->callback_param = pdata;

	/* start DMA */
	pdata->tx_cookie = dmaengine_submit(dmadesc);
	if (dma_submit_error(pdata->tx_cookie))
		dev_err(&pdev->dev, "tx_cookie error on dmaengine_submit\n");
	dma_async_issue_pending(dmachan);

	return 0;
}

static void fpga_dma_dma_shutdown(struct fpga_dma_pdata *pdata)
{
	if (pdata->txchan) {
		dmaengine_terminate_all(pdata->txchan);
		dma_release_channel(pdata->txchan);
	}
	if (pdata->rxchan) {
		dmaengine_terminate_all(pdata->rxchan);
		dma_release_channel(pdata->rxchan);
	}
	pdata->rxchan = pdata->txchan = NULL;
}

static int fpga_dma_dma_init(struct fpga_dma_pdata *pdata)
{
	struct platform_device *pdev = pdata->pdev;

	pdata->txchan = dma_request_slave_channel(&pdev->dev, "tx");
	if (pdata->txchan){
		dev_dbg(&pdev->dev, "TX channel %s %d selected\n",
			dma_chan_name(pdata->txchan), pdata->txchan->chan_id);
		printk("TX channel %s %d selected\n", dma_chan_name(pdata->txchan), pdata->txchan->chan_id);
	}
	else
		dev_err(&pdev->dev, "could not get TX dma channel\n");

	pdata->rxchan = dma_request_slave_channel(&pdev->dev, "rx");
	if (pdata->rxchan)
		dev_dbg(&pdev->dev, "RX channel %s %d selected\n",
			dma_chan_name(pdata->rxchan), pdata->rxchan->chan_id);
	else
		dev_err(&pdev->dev, "could not get RX dma channel\n");

	if (!pdata->rxchan && !pdata->txchan)
		/* both channels not there, maybe it's
		   bcs dma isn't loaded... */
		return -EPROBE_DEFER;

	if (!pdata->rxchan || !pdata->txchan)
		return -ENOMEM;

	return 0;
}

/* --------------------------------------------------------------------- */

static void __iomem *request_and_map(struct platform_device *pdev,
				     const struct resource *res)
{
	void __iomem *ptr;

	if (!devm_request_mem_region(&pdev->dev, res->start, resource_size(res),
				     pdev->name)) {
		dev_err(&pdev->dev, "unable to request %s\n", res->name);
		return NULL;
	}

	ptr = devm_ioremap_nocache(&pdev->dev, res->start, resource_size(res));
	if (!ptr)
		dev_err(&pdev->dev, "ioremap_nocache of %s failed!", res->name);

	return ptr;
}

static int fpga_dma_remove(struct platform_device *pdev)
{
	struct fpga_dma_pdata *pdata = platform_get_drvdata(pdev);
	dev_dbg(&pdev->dev, "fpga_dma_remove\n");
	debugfs_remove_recursive(pdata->root);
	fpga_dma_dma_shutdown(pdata);
	return 0;
}

static int fpga_dma_probe(struct platform_device *pdev)
{
	struct resource *csr_reg, *data_reg;
	struct fpga_dma_pdata *pdata;
	int ret;

	pdata = devm_kzalloc(&pdev->dev, sizeof(struct fpga_dma_pdata),
			     GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	csr_reg = platform_get_resource_byname(pdev, IORESOURCE_MEM, "csr");
	data_reg = platform_get_resource_byname(pdev, IORESOURCE_MEM, "data");
	if (!csr_reg || !data_reg) {
		dev_err(&pdev->dev, "registers not completely defined\n");
		return -EINVAL;
	}

	pdata->csr_reg = request_and_map(pdev, csr_reg);
	if (!pdata->csr_reg)
		return -ENOMEM;

	pdata->data_reg = request_and_map(pdev, data_reg);
	if (!pdata->data_reg)
		return -ENOMEM;
	pdata->data_reg_phy = data_reg->start;

	/* read HW and calculate fifo size in bytes */
	pdata->fifo_depth = readl(pdata->csr_reg + ALT_FPGADMA_CSR_FIFO_DEPTH);
	pdata->data_width = readl(pdata->csr_reg + ALT_FPGADMA_CSR_DATA_WIDTH);
	/* 64-bit bus to FIFO */
	pdata->data_width_bytes = pdata->data_width / sizeof(u64);
	pdata->fifo_size_bytes = pdata->fifo_depth * pdata->data_width_bytes;

	pdata->read_buf = devm_kzalloc(&pdev->dev, pdata->fifo_size_bytes,
				       GFP_KERNEL);
	if (!pdata->read_buf)
		return -ENOMEM;

	pdata->write_buf = devm_kzalloc(&pdev->dev, pdata->fifo_size_bytes,
					GFP_KERNEL);
	if (!pdata->write_buf)
		return -ENOMEM;

	ret = fpga_dma_register_dbgfs(pdata);
	if (ret)
		return ret;

	pdata->pdev = pdev;
	platform_set_drvdata(pdev, pdata);

	ret = fpga_dma_dma_init(pdata);
	if (ret) {
		fpga_dma_remove(pdev);
		return ret;
	}

	/* OK almost ready, set up the watermarks */
	/* we may need to tweak this for single/burst, etc */
	writel(100, pdata->data_reg+ALT_FPGADMA_DATA_WRITE);
	writel(200, pdata->data_reg+ALT_FPGADMA_DATA_WRITE);
	int temp = readl(pdata->data_reg+ALT_FPGADMA_DATA_READ);
	printk("data is: %d\n", temp);
	writel(pdata->fifo_depth - max_burst_words,
	       pdata->csr_reg + ALT_FPGADMA_CSR_WR_WTRMK);
	/* we use read watermark of 0 so that rx_burst line
	   is always asserted, i.e. no single-only requests */
	writel(0, pdata->csr_reg + ALT_FPGADMA_CSR_RD_WTRMK);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id fpga_dma_of_match[] = {
	{.compatible = "altr,fpga-dma",},
	{},
};

MODULE_DEVICE_TABLE(of, fpga_dma_of_match);
#endif

static struct platform_driver fpga_dma_driver = {
	.probe = fpga_dma_probe,
	.remove = fpga_dma_remove,
	.driver = {
		   .name = "fpga_dma",
		   .owner = THIS_MODULE,
		   .of_match_table = of_match_ptr(fpga_dma_of_match),
		   },
};

static int __init fpga_dma_init(void)
{
	return platform_driver_probe(&fpga_dma_driver, fpga_dma_probe);
}

static void __exit fpga_dma_exit(void)
{
	platform_driver_unregister(&fpga_dma_driver);
}

late_initcall(fpga_dma_init);
module_exit(fpga_dma_exit);

MODULE_AUTHOR("Graham Moore (Altera)");
MODULE_DESCRIPTION("Altera FPGA DMA Example Driver");
MODULE_LICENSE("GPL v2");
