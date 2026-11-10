#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/completion.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/ktime.h>
#include <linux/delay.h>
struct hcsr04_dev {
	struct gpio_desc *trig;
	struct gpio_desc *echo;
	int irq;
	ktime_t rise_time;
	u64 distance_mm;
	struct completion measurement_done;
	struct mutex lock;
	struct miscdevice miscdev;
};

static irqreturn_t hcsr04_irq(int irq, void *data)
{
	struct hcsr04_dev *hdev = data;
	int level = gpiod_get_value(hdev->echo);
	if (level > 0) {
		hdev->rise_time = ktime_get();
	} else {
		s64 delta_ns = ktime_to_ns(
			ktime_sub(ktime_get(), hdev->rise_time));
		if (delta_ns > 0) {
			hdev->distance_mm =
				div_u64((u64)delta_ns * 343ULL, 2000000ULL);
			complete(&hdev->measurement_done);
		}
	}

	return IRQ_HANDLED;
}

static ssize_t hcsr04_read(struct file *file,
	char __user *buf,
	size_t count,
	loff_t *ppos)
{
	struct miscdevice *misc = file->private_data;
	struct hcsr04_dev *hdev =
		container_of(misc, struct hcsr04_dev, miscdev);
	char out[32];
	int len;
	long timeout;

	if (*ppos != 0)
		return 0;

	mutex_lock(&hdev->lock);
	reinit_completion(&hdev->measurement_done);

	gpiod_set_value_cansleep(hdev->trig, 0);
	udelay(2);
	gpiod_set_value_cansleep(hdev->trig, 1);
	udelay(10);
	gpiod_set_value_cansleep(hdev->trig, 0);
	
	timeout = wait_for_completion_timeout(
		&hdev->measurement_done,
		msecs_to_jiffies(100));

	if (!timeout) {
		mutex_unlock(&hdev->lock);
		return -ETIMEDOUT;
	}

	len = scnprintf(out, sizeof(out), "%llu\n",
		hdev->distance_mm);

	mutex_unlock(&hdev->lock);
 
	return simple_read_from_buffer(
		buf, count, ppos, out, len);
}

static const struct file_operations hcsr04_fops = {
	.owner = THIS_MODULE,
	.read = hcsr04_read,
};

static int hcsr04_probe(struct platform_device *pdev)
{
	struct hcsr04_dev *hdev;
	int ret;

	hdev = devm_kzalloc(&pdev->dev,
			sizeof(*hdev),
			GFP_KERNEL);
	if (!hdev)
		return -ENOMEM;

	hdev->trig = devm_gpiod_get(
		&pdev->dev, "trig", GPIOD_OUT_LOW);

	if (IS_ERR(hdev->trig))
		return dev_err_probe(
			&pdev->dev, PTR_ERR(hdev->trig),
			"erro ao obter GPIO TRIG\n");

	hdev->echo = devm_gpiod_get(
		&pdev->dev, "echo", GPIOD_IN);
	if (IS_ERR(hdev->echo))
		return dev_err_probe(
			&pdev->dev, PTR_ERR(hdev->echo),
			"erro ao obter GPIO ECHO\n");

	hdev->irq = gpiod_to_irq(hdev->echo);
	if (hdev->irq < 0)
		return hdev->irq;

	init_completion(&hdev->measurement_done);
	mutex_init(&hdev->lock);

	ret = devm_request_irq(
		&pdev->dev,
		hdev->irq,
		hcsr04_irq,
		IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
		dev_name(&pdev->dev),
		hdev);
	if (ret)
		return dev_err_probe(
			&pdev->dev, ret,
			"erro ao registrar IRQ\n");

	hdev->miscdev.minor = MISC_DYNAMIC_MINOR;
	hdev->miscdev.name = "hcsr04";
	hdev->miscdev.fops = &hcsr04_fops;
	hdev->miscdev.parent = &pdev->dev;

	ret = misc_register(&hdev->miscdev);
	if (ret)
		return ret;

	platform_set_drvdata(pdev, hdev);

	dev_info(&pdev->dev,
		"HC-SR04 registrado em /dev/hcsr04\n");

	return 0;
}

static void hcsr04_remove(struct platform_device *pdev)
{
	struct hcsr04_dev *hdev =
		platform_get_drvdata(pdev);
	
	misc_deregister(&hdev->miscdev);
}

static const struct of_device_id hcsr04_of_match[] = {
 	{ .compatible = "unisanta,hcsr04" },
 	{ }
};
MODULE_DEVICE_TABLE(of, hcsr04_of_match);

static struct platform_driver hcsr04_driver = {
	.probe = hcsr04_probe,
	.remove = hcsr04_remove,
	.driver = {
		.name = "hcsr04",
		.of_match_table = hcsr04_of_match,
	},
};

module_platform_driver(hcsr04_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Grupo TP9 - Sistemas Embarcados");
MODULE_DESCRIPTION("Driver Linux para HC-SR04");
