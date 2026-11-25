// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define pr_fmt(fmt)     "motUtil:[%s]" fmt, __func__

#include <linux/debugfs.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/err.h>

#include "sde_kms.h"
#include "sde_connector.h"
#include "sde_encoder.h"
#include "sde_motUtil.h"
#include "dsi_display.h"


#define MAIN_DSI_CONN_NAME "DSI-1"
#define CLI_DSI_CONN_NAME "DSI-2"

static struct motUtil motUtil_data;
static u8 mot_util_read_buf[MAX_CMD_PAYLOAD_SIZE];

static ssize_t _sde_debugfs_motUtil_string2num(const char __user *buf,
					size_t count, char buffer[])
{
	char *input, *cursor, *token;
	u32 buf_size = 0;
	int rc = 0, strtoint;

	if (!count || count > MAX_CMD_PAYLOAD_SIZE * 6)
		return -E2BIG;

	/*copy the data from user space*/
	input = kmalloc(count + 1, GFP_KERNEL);
	if (!input)
		return -ENOMEM;

	if (copy_from_user(input, buf, count)) {
		SDE_ERROR("copy from user failed\n");
		rc = -EFAULT;
		goto end;
	}
	input[count] = '\0';
	SDE_INFO("input_cmd = %s\n", input);

	cursor = input;
	while ((token = strsep(&cursor, " \t\n"))) {
		if (!*token)
			continue;
		rc = kstrtoint(token, 0, &strtoint);
		if (rc || strtoint < 0 || strtoint > 0xff) {
			rc = -EINVAL;
			SDE_ERROR("input buffer conversion failed\n");
			goto end;
		}

		if (buf_size >= MAX_CMD_PAYLOAD_SIZE) {
			SDE_ERROR("buffer size exceeding the limit %d\n",
					MAX_CMD_PAYLOAD_SIZE);
			rc = -E2BIG;
			goto end;
		}

		buffer[buf_size++] = strtoint;
	}

	SDE_DEBUG("command packet size in bytes: %u\n", buf_size);
	rc = buf_size;
end:
	kfree(input);

	return rc;
}

static struct drm_connector *_sde_debugfs_motUtil_get_drm_conn(
					struct sde_kms *kms,
					size_t count, char input[])
{
	struct drm_device *dev = kms->dev;
	struct drm_connector *conn = NULL;
	int found_conn = 0;
	u8 panel_type = input[DISPUTIL_PANEL_TYPE];
	struct drm_connector_list_iter conn_iter;

	mutex_lock(&dev->mode_config.mutex);
	drm_connector_list_iter_begin(dev, &conn_iter);
	drm_for_each_connector_iter(conn, &conn_iter) {
		if (panel_type == MOTUTIL_MAIN_DISP) {
			if (!strncmp(conn->name, MAIN_DSI_CONN_NAME, 5)) {
				SDE_DEBUG("found MAIN disp conn\n");
				found_conn = 1;
				drm_connector_get(conn);
				break;
			}
		} else if (panel_type == MOTUTIL_CLI_DISP) {
			if (!strncmp(conn->name, CLI_DSI_CONN_NAME, 5)) {
				SDE_DEBUG("found CLI disp conn\n");
				found_conn = 1;
				drm_connector_get(conn);
				break;
			}
		}
	}

	drm_connector_list_iter_end(&conn_iter);
	mutex_unlock(&dev->mode_config.mutex);

	if (found_conn)
		return conn;
	else
		return NULL;
}

static struct sde_connector *_sde_debugfs_motUtil_get_sde_conn(
						struct sde_kms *kms,
						size_t count, char input[])
{
	struct drm_connector *conn = NULL;
	struct sde_connector *sde_conn = NULL;

	conn = _sde_debugfs_motUtil_get_drm_conn(kms, count, input);
	if (conn)
		sde_conn = to_sde_connector(conn);
	else
		DRM_ERROR("Can not find SDE Connector for %s\n",
			(input[DISPUTIL_PANEL_TYPE] == MOTUTIL_MAIN_DISP) ?
			"Main Display" : "CLI Display");

	return sde_conn;
}

static int _sde_debugfs_motUtil_transfer(struct sde_kms *kms,
					size_t count, char input[])
{
	struct sde_connector *sde_conn = NULL;
	int ret = 0;
	u8 panel_type = input[DISPUTIL_PANEL_TYPE];

	if (panel_type > MOTUTIL_CLI_DISP) {
		SDE_ERROR("Invalid panel number = %d\n", panel_type);
		return -EINVAL;
	}
	if (count < DISPUTIL_PAYLOAD)
		return -EINVAL;

	sde_conn = _sde_debugfs_motUtil_get_sde_conn(kms, count, input);
	if (!sde_conn) {
		ret = -EINVAL;
		goto end;
	}

	if (sde_conn && !sde_conn->ops.motUtil_transfer) {
		SDE_ERROR("no motUtil_transfer support for connector name %s\n",
						sde_conn->name);
		ret = -ENOENT;
		goto end;
	}

	if (motUtil_data.rd_buf) {
		mutex_lock(&sde_conn->lock);
		ret = sde_conn->ops.motUtil_transfer(sde_conn->display,
						input,	count, &motUtil_data);
		motUtil_data.cmd_status = ret;
		motUtil_data.read_len = ret > 0 && motUtil_data.read_cmd ?
			min_t(int, ret, (u8)input[DISPUTIL_NUM_BYTE_RD]) : 0;
		mutex_unlock(&sde_conn->lock);
	} else
		ret = -ENOMEM;
end:
	if (sde_conn)
		drm_connector_put(&sde_conn->base);
	return ret;
}


static int _sde_debugfs_motUtil_kms_prop_test(struct sde_kms *kms,
					size_t count, char input[])
{
	struct drm_connector *conn = NULL;
	struct sde_connector *sde_conn = NULL;
	struct drm_property *property;

	int ret = 0;
	enum msm_mdp_conn_property conn_prop_idx;
	uint64_t prop_val;

	if (count <= KMSPROPTEST_PROP_INDEX)
		return -EINVAL;

	conn = _sde_debugfs_motUtil_get_drm_conn(kms, count, input);
	if (!conn) {
		ret = -EINVAL;
		goto end;
	}

	sde_conn = to_sde_connector(conn);
	if (!sde_conn) {
		ret = -EINVAL;
		goto end;
	}

	if (count <= KMSPROPTEST_PROP_INDEX ||
	    (input[KMSPROPTEST_PROP_TYPE] == KMSPROPTEST_SETPROP &&
	     count <= KMSPROPTEST_NEW_VAL)) {
		DRM_ERROR(" Number of Input data is invalid (%zu).\n", count);
		ret = -EINVAL;
		goto end;
	}

	switch (input[KMSPROPTEST_PROP_INDEX]) {
	case KMSPROPTEST_TYPE_HBM:
			conn_prop_idx = CONNECTOR_PROP_HBM;
			break;
	case KMSPROPTEST_TYPE_ACL:
			conn_prop_idx = CONNECTOR_PROP_ACL;
			break;
	case KMSPROPTEST_TYPE_CABC:
			conn_prop_idx = CONNECTOR_PROP_CABC;
			break;
	case KMSPROPTEST_TYPE_DC:
			conn_prop_idx = CONNECTOR_PROP_DC;
			break;
	case KMSPROPTEST_TYPE_COLOR:
			conn_prop_idx = CONNECTOR_PROP_COLOR;
			break;
	default:
			DRM_ERROR(" Invalid KMSPROPTEST_PROP_INDEX = %d\n",
						input[KMSPROPTEST_PROP_INDEX]);
			ret = -EINVAL;
			break;
	}

	if (ret)
		goto end;

	property = msm_property_index_to_drm_property(&sde_conn->property_info,
							conn_prop_idx);
	if (!property) {
		SDE_ERROR("invalid property index %d for sde_conn_name=%s\n",
				conn_prop_idx, sde_conn->name);
		ret = -EINVAL;
		goto end;
	}

	prop_val = sde_connector_get_property(conn->state, conn_prop_idx);
	if (prop_val == PARAM_STATE_DISABLE) {
		DRM_ERROR("This KMS Property=%d is disabled\n", conn_prop_idx);
		ret = -EPERM;
		goto end;
	}

	switch (input[KMSPROPTEST_PROP_TYPE]) {
	case KMSPROPTEST_GETPROP:
			motUtil_data.read_cmd = true;
			motUtil_data.val = (int)prop_val;
			break;
	case KMSPROPTEST_SETPROP:
			motUtil_data.read_cmd = false;
			if (conn->funcs && conn->funcs->atomic_set_property) {
				ret = conn->funcs->atomic_set_property(conn,
					conn->state, property,
					input[KMSPROPTEST_NEW_VAL]);
			} else {
				ret = -EINVAL;
				DRM_ERROR("Unsupport KMS set property\n");
			}

			break;
	default:
			DRM_ERROR("Invalid KMSPROPTEST_PROP_TYPE = %d\n",
					input[KMSPROPTEST_PROP_TYPE]);
			ret = -EINVAL;
			break;
	}

end:
	motUtil_data.cmd_status = ret;
	if (conn)
		drm_connector_put(conn);
	return ret;
}

static ssize_t _sde_debugfs_motUtil_exe_command(struct sde_kms *kms,
					size_t count, char input[])
{
	int ret = 0;
	enum sde_motUtil_type motUtil_type;

	if (count < 2)
		return -EINVAL;
	motUtil_type = input[0];

	motUtil_data.motUtil_type = motUtil_type;
	switch (motUtil_type) {
	case MOTUTIL_DISP_UTIL:
		ret = _sde_debugfs_motUtil_transfer(kms, count, input);
		break;
	case MOTUTIL_KMS_PROP_TEST:
		ret = _sde_debugfs_motUtil_kms_prop_test(kms, count, input);
		break;
	default:
		SDE_ERROR("Un-support motUtil type = %d\n", motUtil_type);
		ret = -EINVAL;
	}

	return ret;
}

static int _sde_debugfs_motUtil_open(struct inode *inode,
				struct file *file)
{
	if (!inode || !file)
		return -EINVAL;

	/* non-seekable */
	file->private_data = inode->i_private;
	return nonseekable_open(inode, file);
}

static ssize_t _sde_debugfs_motUtil_write(struct file *file,
			const char __user *buf, size_t count, loff_t *ppos)
{
	ssize_t buf_size;
	char buffer[MAX_CMD_PAYLOAD_SIZE];
	struct sde_kms *kms = (struct sde_kms *)file->private_data;
	int ret = 0;

	if (*ppos || !kms) {
		SDE_ERROR("invalid argument(s)\n");
		ret = -EINVAL;
		goto end;
	}

	buf_size = _sde_debugfs_motUtil_string2num(buf, count, buffer);
	if (buf_size <= 0) {
		SDE_ERROR("invalid input\n");
		ret = buf_size ? buf_size : -EINVAL;
		goto end;
	}

	mutex_lock(&motUtil_data.lock);
	ret = _sde_debugfs_motUtil_exe_command(kms, buf_size, buffer);
	motUtil_data.cmd_status = ret;
	mutex_unlock(&motUtil_data.lock);
end:
	return ret < 0 ? ret : count;
}

static ssize_t _sde_debugfs_motUtil_dispUtil_read(char *buffer, size_t size)
{
	int blen = 0, i;

	if (motUtil_data.read_len) {
		blen = scnprintf(buffer, size, "motUtil_read: ");
		for (i = 0; i < motUtil_data.read_len && blen < size; i++)
			blen += scnprintf(buffer + blen, size - blen,
				"0x%02x ", motUtil_data.rd_buf[i]);
		blen += scnprintf(buffer + blen, size - blen, "\n");
	} else
		SDE_ERROR("motUtil failed to read from panel\n");

	return blen;
}

static ssize_t _sde_debugfs_motUtil_read(struct file *file,
			char __user *buf, size_t count, loff_t *ppos)
{
	int blen = 0;
	ssize_t ret;
	char buffer[MAX_CMD_PAYLOAD_SIZE * 5 + 32];

	mutex_lock(&motUtil_data.lock);
	if (motUtil_data.cmd_status < 0 || !motUtil_data.read_cmd) {
		blen = scnprintf(buffer, sizeof(buffer),
					"motUtil_status: %d\n",
					motUtil_data.cmd_status);
	} else if (motUtil_data.motUtil_type == MOTUTIL_DISP_UTIL) {
		blen = _sde_debugfs_motUtil_dispUtil_read(buffer,
						     sizeof(buffer));
	} else if (motUtil_data.motUtil_type == MOTUTIL_KMS_PROP_TEST) {
		blen = scnprintf(buffer, sizeof(buffer),
				"motUtil_read: 0x%08x\n", motUtil_data.val);
	}
	ret = simple_read_from_buffer(buf, count, ppos, buffer, blen);
	mutex_unlock(&motUtil_data.lock);
	return ret;
}

static const struct file_operations sde_debugfs_motUtil_fops = {
	.owner = THIS_MODULE,
	.open =         _sde_debugfs_motUtil_open,
	.read =         _sde_debugfs_motUtil_read,
	.write =        _sde_debugfs_motUtil_write,
};

int sde_debugfs_mot_util_init(struct sde_kms *sde_kms,
			struct dentry *parent)
{
	struct dentry *entry;

	motUtil_data.rd_buf = mot_util_read_buf;
	motUtil_data.te_enable = true;
	mutex_init(&motUtil_data.lock);
	entry = debugfs_create_file("motUtil", 0600, parent,
			sde_kms, &sde_debugfs_motUtil_fops);
	if (IS_ERR(entry))
		return PTR_ERR(entry);
	if (!entry)
		return -ENOMEM;
	return 0;
}
