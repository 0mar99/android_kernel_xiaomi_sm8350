/*
 * Copyright (c) 2020 The Linux Foundation. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * DOC: Declare private API which shall be used internally only
 * in pkt_capture component. This file shall include prototypes of
 * pkt_capture data tx and data rx.
 *
 * Note: This API should be never accessed out of pkt_capture component.
 */

#ifndef _WLAN_PKT_CAPTURE_DATA_TXRX_H_
#define _WLAN_PKT_CAPTURE_DATA_TXRX_H_

#include "cdp_txrx_cmn_struct.h"
#include <ol_txrx_types.h>
#include <qdf_nbuf.h>

/**
 * pkt_capture_data_process_type - data pkt types to process
 * for packet capture mode
 * @TXRX_PROCESS_TYPE_DATA_RX: process RX packets (normal rx + offloaded rx)
 * @TXRX_PROCESS_TYPE_DATA_TX: process TX packets (ofloaded tx)
 * @TXRX_PROCESS_TYPE_DATA_TX_COMPL: process TX compl packets (normal tx)
 */
enum pkt_capture_data_process_type {
	TXRX_PROCESS_TYPE_DATA_RX,
	TXRX_PROCESS_TYPE_DATA_TX,
	TXRX_PROCESS_TYPE_DATA_TX_COMPL,
};

#define TXRX_PKTCAPTURE_PKT_FORMAT_8023    0
#define TXRX_PKTCAPTURE_PKT_FORMAT_80211   1

/**
 * pkt_capture_datapkt_process() - process data tx and rx packets
 * for pkt capture mode. (normal tx/rx + offloaded tx/rx)
 * @vdev_id: vdev id for which packet is captured
 * @mon_buf_list: netbuf list
 * @type: data process type
 * @tid:  tid number
 * @status: Tx status
 * @pkt_format: Frame format
 * @bssid: bssid
 * @pdev: pdev handle
 *
 * Return: none
 */
void pkt_capture_datapkt_process(
			uint8_t vdev_id,
			qdf_nbuf_t mon_buf_list,
			enum pkt_capture_data_process_type type,
			uint8_t tid, uint8_t status, bool pkt_format,
			uint8_t *bssid, htt_pdev_handle pdev);
/**
 * pkt_capture_msdu_process_pkts() - process data rx pkts
 * @bssid: bssid
 * @head_msdu: pointer to head msdu
 * @vdev_id: vdev_id
 * @pdev: pdev handle
 *
 * Return: none
 */
void pkt_capture_msdu_process_pkts(
			uint8_t *bssid,
			qdf_nbuf_t head_msdu,
			uint8_t vdev_id,
			htt_pdev_handle pdev);

/**
 * pkt_capture_rx_in_order_drop_offload_pkt() - drop offload packets
 * @head_msdu: pointer to head msdu
 *
 * Return: none
 */
void pkt_capture_rx_in_order_drop_offload_pkt(qdf_nbuf_t head_msdu);

/**
 * pkt_capture_rx_in_order_offloaded_pkt() - check offloaded data pkt or not
 * @rx_ind_msg: rx_ind_msg
 *
 * Return: false, if it is not offload pkt
 *         true, if it is offload pkt
 */
bool pkt_capture_rx_in_order_offloaded_pkt(qdf_nbuf_t rx_ind_msg);
#endif /* End  of _WLAN_PKT_CAPTURE_DATA_TXRX_H_ */
