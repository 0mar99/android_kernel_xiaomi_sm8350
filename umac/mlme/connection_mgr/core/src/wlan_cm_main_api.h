/*
 * Copyright (c) 2012-2015, 2020, The Linux Foundation. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * DOC: wlan_cm_main_api.h
 *
 * This header file maintain connect, disconnect APIs of connection manager
 */

#ifndef __WLAN_CM_MAIN_API_H__
#define __WLAN_CM_MAIN_API_H__

#include "wlan_cm_main.h"

/*************** CONNECT APIs ****************/
/**
 * cm_connect_start() - This API will be called to initiate the connect
 * process
 * @cm_ctx: connection manager context
 * @req: Connect request.
 *
 * Return: QDF status
 */
QDF_STATUS cm_connect_start(struct cnx_mgr *cm_ctx, struct cm_connect_req *req);

/**
 * cm_connect_scan_start() - This API will be called to initiate the connect
 * scan if no candidate are found in scan db.
 * @cm_ctx: connection manager context
 * @req: Connect request.
 *
 * Return: QDF status
 */
QDF_STATUS cm_connect_scan_start(struct cnx_mgr *cm_ctx,
				 struct cm_connect_req *req);

/**
 * cm_connect_scan_resp() - Handle the connect scan resp and next action
 * scan if no candidate are found in scan db.
 * @scan_id: scan id of the req
 * @status: Connect scan status
 *
 * Return: QDF status
 */
QDF_STATUS cm_connect_scan_resp(struct cnx_mgr *cm_ctx, wlan_scan_id *scan_id,
				QDF_STATUS status);

/**
 * cm_connect_active() - This API would be called after the connect
 * request gets activated in serialization.
 * @cm_ctx: connection manager context
 * @cm_id: Connection mgr ID assigned to this connect request.
 *
 * Return: QDF status
 */
QDF_STATUS cm_connect_active(struct cnx_mgr *cm_ctx, wlan_cm_id *cm_id);

/**
 * cm_try_next_candidate() - This API would try to connect to next valid
 * candidate and fail if no candidate left
 * request gets activated in serialization.
 * @cm_ctx: connection manager context
 * @connect_resp: connect resp.
 *
 * Return: QDF status
 */
QDF_STATUS cm_try_next_candidate(struct cnx_mgr *cm_ctx,
				 struct wlan_cm_connect_rsp *connect_resp);

/**
 * cm_connect_cmd_timeout() - Called if active connect command timeout
 * @cm_ctx: connection manager context
 * @cm_id: Connection mgr ID assigned to this connect request.
 *
 * Return: QDF status
 */
QDF_STATUS cm_connect_cmd_timeout(struct cnx_mgr *cm_ctx, wlan_cm_id cm_id);

/**
 * cm_connect_complete() - This API would be called after connect complete
 * request from the serialization.
 * @cm_ctx: connection manager context
 * @resp: Connection complete resp.
 *
 * This API would be called after connection completion resp from VDEV mgr
 *
 * Return: QDF status
 */
QDF_STATUS cm_connect_complete(struct cnx_mgr *cm_ctx,
			       struct wlan_cm_connect_rsp *resp);

/**
 * cm_connect_start_req() - Connect start req from the requester
 * @vdev: vdev on which connect is received
 * @req: Connection req provided
 *
 * Return: QDF status
 */
QDF_STATUS cm_connect_start_req(struct wlan_objmgr_vdev *vdev,
				struct wlan_cm_connect_req *req);

/*************** DISCONNECT APIs ****************/

/**
 * cm_disconnect_start() - Initiate the disconnect process
 * @cm_ctx: connection manager context
 * @req: Disconnect request.
 *
 * Return: QDF status
 */
QDF_STATUS cm_disconnect_start(struct cnx_mgr *cm_ctx,
			       struct cm_disconnect_req *req);

/**
 * cm_disconnect_active() - This API would be called after the disconnect
 * request gets activated in serialization.
 * @cm_ctx: connection manager context
 * @cm_id: Connection mgr ID assigned to this connect request.
 *
 * Return: QDF status
 */
QDF_STATUS cm_disconnect_active(struct cnx_mgr *cm_ctx, wlan_cm_id *cm_id);

/**
 * cm_disconnect_complete() - This API would be called after disconnect complete
 * request from the serialization.
 * @cm_ctx: connection manager context
 * @resp: disconnection complete resp.
 *
 * This API would be called after connection completion resp from VDEV mgr
 *
 * Return: QDF status
 */
QDF_STATUS cm_disconnect_complete(struct cnx_mgr *cm_ctx,
				  struct wlan_cm_discon_rsp *resp);

/**
 * cm_disconnect_start_req() - Disconnect start req from the requester
 * @vdev: vdev on which connect is received
 * @req: disconnection req provided
 *
 * Return: QDF status
 */
QDF_STATUS cm_disconnect_start_req(struct wlan_objmgr_vdev *vdev,
				   struct wlan_cm_disconnect_req *req);

#endif /* __WLAN_CM_MAIN_API_H__ */
