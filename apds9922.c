#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/sysfs.h>

#define MAIN_CTRL 0X00
#define PRX_LED_ADDR 0x01
#define PRX_PULSES_ADDR 0x02
#define PRX_MEAS_RATE_ADDR 0x03
#define ALS_MEAS_RATE_ADDR 0x04
#define ALS_GAIN_RANGE_ADDR 0X05
#define PART_ID_ADDR 0X06
#define MAIN_STATUS_ADDR 0x07
#define PRX_DATA_0_ADDR 0x08
#define PRX_DATA_1_ADDR 0x09
#define ALS_DATA_0_ADDR 0x0D
#define ALS_DATA_1_ADDR 0x0E
#define ALS_DATA_2_ADDR 0x0F
#define INT_CFG_ADDR 0x19
#define INT_PERSISTENCE_ADDR 0x1A
#define COMP_DATA_0_ADDR 0x16
#define COMP_DATA_1_ADDR 0x17
#define COMP_DATA_2_ADDR 0x18
#define PRX_THRES_UP_0_ADDR 0x1B
#define PRX_THRES_UP_1_ADDR 0x1C
#define PRX_THRES_LOW_0_ADDR 0x1D
#define PRX_THRES_LOW_1_ADDR 0x1E
#define PRX_CAN_0_ADDR 0x1F
#define PRX_CAN_1_ADDR 0x20
#define DEVICE_CONFIG_ADDR 0x2F

#define DRIVER_NAME "apds9922"

struct apds9922_data
{

	struct i2c_client *client;
	int als_data;
	int16_t prx_data;
	struct mutex lock;
};

static ssize_t apds9922_als_data_read(struct device *dev, struct device_attribute *attr, const char *buf)
{

	struct i2c_client *client = to_i2c_client(dev);
	struct apds9922_data *data = i2c_get_clientdata(client);
	uint8_t als_data_seg[3];

	mutex_lock(&data->lock);

	int als_data;

	als_data_seg[0] = i2c_smbus_read_byte_data(client, ALS_DATA_0_ADDR);
	if (als_data_seg[0] < 0)
	{
		dev_err(&client->dev, "read register failed\n");
		return -1;
	}

	als_data_seg[1] = i2c_smbus_read_byte_data(client, ALS_DATA_1_ADDR);
	if (als_data_seg[1] < 0)
	{
		dev_err(&client->dev, "read register failed\n");
		return -1;
	}

	als_data_seg[2] = i2c_smbus_read_byte_data(client, ALS_DATA_2_ADDR);
	if (als_data_seg[2] < 0)
	{
		dev_err(&client->dev, "read register failed\n");
		return -1;
	}

	als_data = (als_data_seg[2] << 16) | (als_data_seg[1] << 8) | als_data_seg[0];

	mutex_unlock(&data->lock);

	sprintf(buf, "%d\n", als_data);

	return als_data;
}

static ssize_t apds9922_prx_data_read(struct device *dev, struct device_attribute *attr, const char *buf)
{

	struct i2c_client *client = to_i2c_client(dev);
	struct apds9922_data *data = i2c_get_clientdata(client);

	int16_t prx_data_seg[2];

	int16_t prx_data;

	mutex_lock(&data->lock);

	prx_data_seg[0] = i2c_smbus_read_byte_data(client, PRX_DATA_0_ADDR);
	if (prx_data_seg[0] < 0)
	{
		dev_err(&client->dev, "read register failed\n");
		return -1;
	}

	prx_data_seg[1] = i2c_smbus_read_byte_data(client, PRX_DATA_1_ADDR);
	if (prx_data_seg[1] < 0)
	{
		dev_err(&client->dev, "read register failed\n");
		return -1;
	}

	prx_data = (prx_data_seg[1] << 8) | prx_data_seg[0];

	mutex_unlock(&data->lock);
	sprintf(buf, "%d\n", prx_data);

	return prx_data;
}

/* als data attribute */
static DEVICE_ATTR(lux_data, S_IRUGO | S_IWUSR, apds9922_als_data_read, NULL);

static struct attribute *att_als[] = {
	&dev_attr_lux_data.attr,
	NULL};

static struct attribute_group als_gr = {
	.name = "apds9922_als",
	.attrs = att_als};

/* px data attribute */
static DEVICE_ATTR(prx_data, S_IRUGO | S_IWUSR, apds9922_prx_data_read, NULL);

static struct attribute *att_prx[] = {
	&dev_attr_prx_data.attr,
	NULL};

static struct attribute_group prx_gr = {
	.name = "apds9922_prx",
	.attrs = att_prx};

static int apds9922_data_init(struct i2c_client *client)
{

	int ret;

	ret = i2c_smbus_write_byte_data(client, MAIN_CTRL, 0X03);
	if (ret < 0)
	{
		dev_err(&client->dev, "write register failed\n");
		return -1;
	}

	ret = i2c_smbus_write_byte_data(client, PRX_LED_ADDR, 0X36);
	if (ret < 0)
	{
		dev_err(&client->dev, "write register failed\n");
		return -1;
	}

	ret = i2c_smbus_write_byte_data(client, PRX_MEAS_RATE_ADDR, 0X5D);
	if (ret < 0)
	{
		dev_err(&client->dev, "write register failed\n");
		return -1;
	}

	ret = i2c_smbus_write_byte_data(client, ALS_MEAS_RATE_ADDR, 0X22);
	if (ret < 0)
	{
		dev_err(&client->dev, "write register failed\n");
		return -1;
	}

	ret = i2c_smbus_write_byte_data(client, ALS_GAIN_RANGE_ADDR, 0X01);
	if (ret < 0)
	{
		dev_err(&client->dev, "write register failed\n");
		return -1;
	}

	return ret;
}
static int apds9922_probe(struct i2c_client *client,
						  const struct i2c_device_id *id)
{
	int res;
	int ret;
	struct apds9922_data *apds9922_data;

	apds9922_data = kzalloc(sizeof(struct apds9922_data), GFP_KERNEL);
	if (apds9922_data == NULL)
	{
		dev_err(&client->dev, "memory allocation failed\n");
		return -ENOMEM;
	}
	apds9922_data->client = client;

	ret = apds9922_data_init(client);
	if (ret < 0)
	{
		dev_err(&client->dev, "device data init failed\n");
	}

	i2c_set_clientdata(client, apds9922_data);
	res = sysfs_create_group(&client->dev.kobj, &als_gr);
	if (res)
	{
		dev_err(&client->dev, "device create file failed\n");
	}

	res = sysfs_create_group(&client->dev.kobj, &prx_gr);
	if (res)
	{
		dev_err(&client->dev, "device create file failed\n");
	}
	dev_info(&client->dev, " apds9922 chip found\n");
	mutex_init(&apds9922_data->lock);
	return 0;
}
static struct i2c_device_id apds9922_id[] = {
	{DRIVER_NAME, 0},
	{}};

MODULE_DEVICE_TABLE(i2c, apds9922_id);

static struct i2c_driver apds9922_driver = {
	.driver = {
		.name = DRIVER_NAME,
	},
	.probe = apds9922_probe,
	.id_table = apds9922_id,
};

module_i2c_driver(apds9922_driver);

MODULE_AUTHOR("Sunny Patel <nueralspacetech@gmail.com>");
MODULE_DESCRIPTION("Avago apds9922 Driver");
MODULE_LICENSE("GPL v2");