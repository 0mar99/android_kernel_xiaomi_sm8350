/*
 * Copyright (c) 2012-2020 The Linux Foundation. All rights reserved.
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

/*
 * DOC: wlan_cm_roam_offload.c
 *
 * Implementation for the common roaming offload api interfaces.
 */

#include "wlan_cm_roam_offload.h"
#include "wlan_cm_tgt_if_tx_api.h"
#include "wlan_cm_roam_api.h"

/**
 * wlan_cm_roam_scan_bmiss_cnt() - set roam beacon miss count
 * @psoc: psoc pointer
 * @vdev_id: vdev id
 * @params: roam beacon miss count parameters
 *
 * This function is used to set roam beacon miss count parameters
 *
 * Return: None
 */
static void
wlan_cm_roam_scan_bmiss_cnt(struct wlan_objmgr_psoc *psoc,
			    uint8_t vdev_id,
			    struct wlan_roam_beacon_miss_cnt *params)
{
	uint8_t beacon_miss_count;

	params->vdev_id = vdev_id;

	wlan_mlme_get_roam_bmiss_first_bcnt(psoc, &beacon_miss_count);
	params->roam_bmiss_first_bcnt = beacon_miss_count;

	wlan_mlme_get_roam_bmiss_final_bcnt(psoc, &beacon_miss_count);
	params->roam_bmiss_final_bcnt = beacon_miss_count;
}

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
/**
 * wlan_cm_roam_reason_vsie() - set roam reason vsie
 * @psoc: psoc pointer
 * @vdev_id: vdev id
 * @params: roam reason vsie parameters
 *
 * This function is used to set roam reason vsie parameters
 *
 * Return: None
 */
static void
wlan_cm_roam_reason_vsie(struct wlan_objmgr_psoc *psoc,
			 uint8_t vdev_id,
			 struct wlan_roam_reason_vsie_enable *params)
{
	uint8_t enable_roam_reason_vsie;

	params->vdev_id = vdev_id;

	wlan_mlme_get_roam_reason_vsie_status(psoc, &enable_roam_reason_vsie);
	params->enable_roam_reason_vsie = enable_roam_reason_vsie;
}

/**
 * wlan_cm_roam_triggers() - set roam triggers
 * @psoc: psoc pointer
 * @vdev_id: vdev id
 * @params: roam triggers parameters
 *
 * This function is used to set roam triggers parameters
 *
 * Return: None
 */
static void
wlan_cm_roam_triggers(struct wlan_objmgr_psoc *psoc,
		      uint8_t vdev_id,
		      struct wlan_roam_triggers *params)
{
	params->vdev_id = vdev_id;
	params->trigger_bitmap =
		mlme_get_roam_trigger_bitmap(psoc, vdev_id);
	wlan_cm_roam_get_vendor_btm_params(psoc, vdev_id,
					   &params->vendor_btm_param);
}
#else
static void
wlan_cm_roam_reason_vsie(struct wlan_objmgr_psoc *psoc,
			 uint8_t vdev_id,
			 struct wlan_roam_reason_vsie_enable *params)
{
}

static void
wlan_cm_roam_triggers(struct wlan_objmgr_psoc *psoc,
		      uint8_t vdev_id,
		      struct wlan_roam_triggers *params)
{
}
#endif

/**
 * cm_roam_init_req() - roam init request handling
 * @psoc: psoc pointer
 * @vdev_id: vdev id
 * @reason: reason for changing roam state for the requested vdev id
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
cm_roam_init_req(struct wlan_objmgr_psoc *psoc,
		 uint8_t vdev_id,
		 bool enable)
{
	return QDF_STATUS_SUCCESS;
}

/**
 * cm_roam_start_req() - roam start request handling
 * @psoc: psoc pointer
 * @vdev_id: vdev id
 * @reason: reason for changing roam state for the requested vdev id
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
cm_roam_start_req(struct wlan_objmgr_psoc *psoc,
		  uint8_t vdev_id,
		  uint8_t reason)
{
	struct wlan_roam_start_config *start_req;
	QDF_STATUS status;

	start_req = qdf_mem_malloc(sizeof(*start_req));
	if (!start_req)
		return QDF_STATUS_E_NOMEM;

	/* fill from mlme directly */
	wlan_cm_roam_scan_bmiss_cnt(psoc, vdev_id,
				    &start_req->beacon_miss_cnt);
	wlan_cm_roam_reason_vsie(psoc, vdev_id, &start_req->reason_vsie_enable);
	wlan_cm_roam_triggers(psoc, vdev_id, &start_req->roam_triggers);

	/* fill from legacy through this API */
	wlan_cm_roam_fill_start_req(psoc, vdev_id, start_req, reason);

	status = wlan_cm_tgt_send_roam_start_req(psoc, vdev_id, start_req);
	if (QDF_IS_STATUS_ERROR(status))
		mlme_debug("fail to send roam start");

	qdf_mem_free(start_req);

	return status;
}

/**
 * cm_roam_update_config_req() - roam update config request handling
 * @psoc: psoc pointer
 * @vdev_id: vdev id
 * @reason: reason for changing roam state for the requested vdev id
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
cm_roam_update_config_req(struct wlan_objmgr_psoc *psoc,
			  uint8_t vdev_id,
			  uint8_t reason)
{
	return QDF_STATUS_SUCCESS;
}

/*
 * similar to csr_roam_offload_scan, will be used from many legacy
 * process directly, generate a new function wlan_cm_roam_send_rso_cmd
 * for external usage.
 */
QDF_STATUS
cm_roam_send_rso_cmd(struct wlan_objmgr_psoc *psoc,
		     uint8_t vdev_id,
		     uint8_t rso_command,
		     uint8_t reason)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	status = wlan_cm_roam_cmd_allowed(psoc, vdev_id, rso_command, reason);

	if (status == QDF_STATUS_E_NOSUPPORT)
		return QDF_STATUS_SUCCESS;
	if (QDF_IS_STATUS_ERROR(status)) {
		mlme_debug("ROAM: not allowed");
		return status;
	}
	if (rso_command == ROAM_SCAN_OFFLOAD_START)
		status = cm_roam_start_req(psoc, vdev_id, reason);
	else if (rso_command == ROAM_SCAN_OFFLOAD_UPDATE_CFG)
		status = cm_roam_update_config_req(psoc, vdev_id, reason);
//	else if (rso_command == ROAM_SCAN_OFFLOAD_RESTART)
		/* RESTART API */
//	else
		/* ABORT SCAN API */

	return status;
}

/**
 * cm_roam_stop_req() - roam stop request handling
 * @psoc: psoc pointer
 * @vdev_id: vdev id
 * @reason: reason for changing roam state for the requested vdev id
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
cm_roam_stop_req(struct wlan_objmgr_psoc *psoc,
		 uint8_t vdev_id,
		 uint8_t reason)
{
	/* do the filling as csr_post_rso_stop */
	return QDF_STATUS_SUCCESS;
}

/**
 * cm_roam_switch_to_rso_stop() - roam state handling for rso stop
 * @pdev: pdev pointer
 * @vdev_id: vdev id
 * @reason: reason for changing roam state for the requested vdev id
 *
 * This function is used for WLAN_ROAM_RSO_STOPPED roam state handling
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
cm_roam_switch_to_rso_stop(struct wlan_objmgr_pdev *pdev,
			   uint8_t vdev_id,
			   uint8_t reason)
{
	enum roam_offload_state cur_state;
	QDF_STATUS status;
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);

	cur_state = mlme_get_roam_state(psoc, vdev_id);
	switch (cur_state) {
	case WLAN_ROAM_RSO_ENABLED:
	case WLAN_ROAMING_IN_PROG:
	case WLAN_ROAM_SYNCH_IN_PROG:
		status = cm_roam_stop_req(psoc, vdev_id, reason);
		if (QDF_IS_STATUS_ERROR(status)) {
			mlme_err("ROAM: Unable to switch to RSO STOP State");
			return QDF_STATUS_E_FAILURE;
		}
		break;

	case WLAN_ROAM_DEINIT:
	case WLAN_ROAM_RSO_STOPPED:
	case WLAN_ROAM_INIT:
	/*
	 * Already the roaming module is initialized at fw,
	 * nothing to do here
	 */
	default:
		return QDF_STATUS_SUCCESS;
	}
	mlme_set_roam_state(psoc, vdev_id, WLAN_ROAM_RSO_STOPPED);

	return QDF_STATUS_SUCCESS;
}

/**
 * cm_roam_switch_to_deinit() - roam state handling for roam deinit
 * @pdev: pdev pointer
 * @vdev_id: vdev id
 * @reason: reason for changing roam state for the requested vdev id
 *
 * This function is used for WLAN_ROAM_DEINIT roam state handling
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
cm_roam_switch_to_deinit(struct wlan_objmgr_pdev *pdev,
			 uint8_t vdev_id,
			 uint8_t reason)
{
	QDF_STATUS status;
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);
	enum roam_offload_state cur_state = mlme_get_roam_state(psoc, vdev_id);

	switch (cur_state) {
	/*
	 * If RSO stop is not done already, send RSO stop first and
	 * then post deinit.
	 */
	case WLAN_ROAM_RSO_ENABLED:
	case WLAN_ROAMING_IN_PROG:
	case WLAN_ROAM_SYNCH_IN_PROG:
		cm_roam_switch_to_rso_stop(pdev, vdev_id, reason);
		break;
	case WLAN_ROAM_RSO_STOPPED:
	case WLAN_ROAM_INIT:
		break;

	case WLAN_ROAM_DEINIT:
	/*
	 * Already the roaming module is de-initialized at fw,
	 * do nothing here
	 */
	default:
		return QDF_STATUS_SUCCESS;
	}

	status = cm_roam_init_req(psoc, vdev_id, false);

	if (QDF_IS_STATUS_ERROR(status))
		return status;

	mlme_set_roam_state(psoc, vdev_id, WLAN_ROAM_DEINIT);

	if (reason != REASON_SUPPLICANT_INIT_ROAMING)
		wlan_cm_enable_roaming_on_connected_sta(pdev, vdev_id);

	return QDF_STATUS_SUCCESS;
}

/**
 * cm_roam_switch_to_init() - roam state handling for roam init
 * @pdev: pdev pointer
 * @vdev_id: vdev id
 * @reason: reason for changing roam state for the requested vdev id
 *
 * This function is used for WLAN_ROAM_INIT roam state handling
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
cm_roam_switch_to_init(struct wlan_objmgr_pdev *pdev,
		       uint8_t vdev_id,
		       uint8_t reason)
{
	enum roam_offload_state cur_state;
	uint8_t temp_vdev_id, roam_enabled_vdev_id;
	uint32_t roaming_bitmap;
	bool dual_sta_roam_active;
	QDF_STATUS status;
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);

	dual_sta_roam_active =
		wlan_mlme_get_dual_sta_roaming_enabled(psoc);

	cur_state = mlme_get_roam_state(psoc, vdev_id);
	switch (cur_state) {
	case WLAN_ROAM_DEINIT:
		roaming_bitmap = mlme_get_roam_trigger_bitmap(psoc, vdev_id);
		if (!roaming_bitmap) {
			mlme_info("ROAM: Cannot change to INIT state for vdev[%d]",
				  vdev_id);
			return QDF_STATUS_E_FAILURE;
		}

		if (dual_sta_roam_active)
			break;
		/*
		 * Disable roaming on the enabled sta if supplicant wants to
		 * enable roaming on this vdev id
		 */
		temp_vdev_id = policy_mgr_get_roam_enabled_sta_session_id(
								psoc, vdev_id);
		if (temp_vdev_id != WLAN_UMAC_VDEV_ID_MAX) {
			/*
			 * Roam init state can be requested as part of
			 * initial connection or due to enable from
			 * supplicant via vendor command. This check will
			 * ensure roaming does not get enabled on this STA
			 * vdev id if it is not an explicit enable from
			 * supplicant.
			 */
			if (reason != REASON_SUPPLICANT_INIT_ROAMING) {
				mlme_info("ROAM: Roam module already initialized on vdev:[%d]",
					  temp_vdev_id);
				return QDF_STATUS_E_FAILURE;
			}
			cm_roam_state_change(pdev, temp_vdev_id,
					     WLAN_ROAM_DEINIT, reason);
		}
		break;

	case WLAN_ROAM_SYNCH_IN_PROG:
		mlme_set_roam_state(psoc, vdev_id, WLAN_ROAM_INIT);
		return QDF_STATUS_SUCCESS;

	case WLAN_ROAM_INIT:
	case WLAN_ROAM_RSO_STOPPED:
	case WLAN_ROAM_RSO_ENABLED:
	case WLAN_ROAMING_IN_PROG:
	/*
	 * Already the roaming module is initialized at fw,
	 * just return from here
	 */
	default:
		return QDF_STATUS_SUCCESS;
	}

	status = cm_roam_init_req(psoc, vdev_id, true);

	if (QDF_IS_STATUS_ERROR(status))
		return status;

	mlme_set_roam_state(psoc, vdev_id, WLAN_ROAM_INIT);

	roam_enabled_vdev_id =
		policy_mgr_get_roam_enabled_sta_session_id(psoc, vdev_id);

	/* Send PDEV pcl command if only one STA is in connected state
	 * If there is another STA connection exist, then set the
	 * PCL type to vdev level
	 */
	if (roam_enabled_vdev_id != WLAN_UMAC_VDEV_ID_MAX &&
	    dual_sta_roam_active)
		wlan_cm_roam_activate_pcl_per_vdev(psoc, vdev_id, true);

	/* Set PCL before sending RSO start */
	policy_mgr_set_pcl_for_existing_combo(psoc, PM_STA_MODE, vdev_id);

	return QDF_STATUS_SUCCESS;
}

/**
 * cm_roam_switch_to_rso_enable() - roam state handling for rso started
 * @pdev: pdev pointer
 * @vdev_id: vdev id
 * @reason: reason for changing roam state for the requested vdev id
 *
 * This function is used for WLAN_ROAM_RSO_ENABLED roam state handling
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
cm_roam_switch_to_rso_enable(struct wlan_objmgr_pdev *pdev,
			     uint8_t vdev_id,
			     uint8_t reason)
{
	enum roam_offload_state cur_state, new_roam_state;
	QDF_STATUS status;
	uint8_t control_bitmap;
	bool sup_disabled_roaming;
	bool rso_allowed;
	uint8_t rso_command = ROAM_SCAN_OFFLOAD_START;
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);

	wlan_mlme_get_roam_scan_offload_enabled(psoc, &rso_allowed);
	sup_disabled_roaming = mlme_get_supplicant_disabled_roaming(psoc,
								    vdev_id);
	control_bitmap = mlme_get_operations_bitmap(psoc, vdev_id);

	cur_state = mlme_get_roam_state(psoc, vdev_id);
	switch (cur_state) {
	case WLAN_ROAM_INIT:
	case WLAN_ROAM_RSO_STOPPED:
		break;

	case WLAN_ROAM_DEINIT:
		status = cm_roam_switch_to_init(pdev, vdev_id, reason);
		if (QDF_IS_STATUS_ERROR(status))
			return status;

		break;
	case WLAN_ROAM_RSO_ENABLED:
		/*
		 * Send RSO update config if roaming already enabled
		 */
		rso_command = ROAM_SCAN_OFFLOAD_UPDATE_CFG;
		break;
	case WLAN_ROAMING_IN_PROG:
		/*
		 * When roam abort happens, the roam offload
		 * state machine moves to RSO_ENABLED state.
		 * But if Supplicant disabled roaming is set in case
		 * of roam invoke or if roaming was disabled due to
		 * other reasons like SAP start/connect on other vdev,
		 * the state should be transitioned to RSO STOPPED.
		 */
		if (sup_disabled_roaming || control_bitmap)
			new_roam_state = WLAN_ROAM_RSO_STOPPED;
		else
			new_roam_state = WLAN_ROAM_RSO_ENABLED;

		mlme_set_roam_state(psoc, vdev_id, new_roam_state);

		return QDF_STATUS_SUCCESS;
	case WLAN_ROAM_SYNCH_IN_PROG:
		/*
		 * After roam sych propagation is complete, send
		 * RSO start command to firmware to update AP profile,
		 * new PCL.
		 * If this is roam invoke case and supplicant has already
		 * disabled firmware roaming, then move to RSO stopped state
		 * instead of RSO enabled.
		 */
		if (sup_disabled_roaming || control_bitmap) {
			new_roam_state = WLAN_ROAM_RSO_STOPPED;
			mlme_set_roam_state(psoc, vdev_id, new_roam_state);

			return QDF_STATUS_SUCCESS;
		}

		break;
	default:
		return QDF_STATUS_SUCCESS;
	}

	if (!rso_allowed) {
		mlme_debug("ROAM: RSO disabled via INI");
		return QDF_STATUS_E_FAILURE;
	}

	if (control_bitmap) {
		mlme_debug("ROAM: RSO Disabled internaly: vdev[%d] bitmap[0x%x]",
			   vdev_id, control_bitmap);
		return QDF_STATUS_E_FAILURE;
	}

	status = cm_roam_send_rso_cmd(psoc, vdev_id, rso_command, reason);
	if (QDF_IS_STATUS_ERROR(status)) {
		mlme_debug("ROAM: RSO start failed");
		return status;
	}
	mlme_set_roam_state(psoc, vdev_id, WLAN_ROAM_RSO_ENABLED);

	/*
	 * If supplicant disabled roaming, driver does not send
	 * RSO cmd to fw. This causes roam invoke to fail in FW
	 * since RSO start never happened at least once to
	 * configure roaming engine in FW. So send RSO start followed
	 * by RSO stop if supplicant disabled roaming is true.
	 */
	if (!sup_disabled_roaming)
		return QDF_STATUS_SUCCESS;

	mlme_debug("ROAM: RSO disabled by Supplicant on vdev[%d]", vdev_id);
	return cm_roam_state_change(pdev, vdev_id, WLAN_ROAM_RSO_STOPPED,
				    REASON_SUPPLICANT_DISABLED_ROAMING);
}

/**
 * cm_roam_switch_to_roam_start() - roam state handling for ROAMING_IN_PROG
 * @pdev: pdev pointer
 * @vdev_id: vdev id
 * @reason: reason for changing roam state for the requested vdev id
 *
 * This function is used for WLAN_ROAMING_IN_PROG roam state handling
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
cm_roam_switch_to_roam_start(struct wlan_objmgr_pdev *pdev,
			     uint8_t vdev_id,
			     uint8_t reason)
{
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);
	enum roam_offload_state cur_state =
				mlme_get_roam_state(psoc, vdev_id);
	switch (cur_state) {
	case WLAN_ROAM_RSO_ENABLED:
		mlme_set_roam_state(psoc, vdev_id, WLAN_ROAMING_IN_PROG);
		break;

	case WLAN_ROAM_RSO_STOPPED:
		/*
		 * When supplicant has disabled roaming, roam invoke triggered
		 * from supplicant can cause firmware to send roam start
		 * notification. Allow roam start in this condition.
		 */
		if (mlme_get_supplicant_disabled_roaming(psoc, vdev_id) &&
		    mlme_is_roam_invoke_in_progress(psoc, vdev_id)) {
			mlme_set_roam_state(psoc, vdev_id,
					    WLAN_ROAMING_IN_PROG);
			break;
		}
	case WLAN_ROAM_INIT:
	case WLAN_ROAM_DEINIT:
	case WLAN_ROAM_SYNCH_IN_PROG:
	default:
		mlme_err("ROAM: Roaming start received in invalid state: %d",
			 cur_state);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * cm_roam_switch_to_roam_sync() - roam state handling for roam sync
 * @pdev: pdev pointer
 * @vdev_id: vdev id
 * @reason: reason for changing roam state for the requested vdev id
 *
 * This function is used for WLAN_ROAM_SYNCH_IN_PROG roam state handling
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
cm_roam_switch_to_roam_sync(struct wlan_objmgr_pdev *pdev,
			    uint8_t vdev_id,
			    uint8_t reason)
{
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);
	struct wlan_objmgr_vdev *vdev;
	enum roam_offload_state cur_state = mlme_get_roam_state(psoc, vdev_id);

	switch (cur_state) {
	case WLAN_ROAM_RSO_ENABLED:
		/*
		 * Roam synch can come directly without roam start
		 * after waking up from power save mode or in case of
		 * deauth roam trigger to stop data path queues
		 */
	case WLAN_ROAMING_IN_PROG:
		vdev = wlan_objmgr_get_vdev_by_id_from_pdev(pdev, vdev_id,
							    WLAN_MLME_NB_ID);
		if (!wlan_vdev_is_up(vdev)) {
			mlme_err("ROAM: STA not in connected state");
			return QDF_STATUS_E_FAILURE;
		}

		mlme_set_roam_state(psoc, vdev_id, WLAN_ROAM_SYNCH_IN_PROG);
		break;
	case WLAN_ROAM_RSO_STOPPED:
		/*
		 * If roaming is disabled by Supplicant and if this transition
		 * is due to roaming invoked by the supplicant, then allow
		 * this state transition
		 */
		if (mlme_get_supplicant_disabled_roaming(psoc, vdev_id) &&
		    mlme_is_roam_invoke_in_progress(psoc, vdev_id)) {
			mlme_set_roam_state(psoc, vdev_id,
					    WLAN_ROAM_SYNCH_IN_PROG);
			break;
		}
	case WLAN_ROAM_INIT:
	case WLAN_ROAM_DEINIT:
	case WLAN_ROAM_SYNCH_IN_PROG:
	default:
		mlme_err("ROAM: Roam synch not allowed in [%d] state",
			 cur_state);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
cm_roam_state_change(struct wlan_objmgr_pdev *pdev,
		     uint8_t vdev_id,
		     enum roam_offload_state requested_state,
		     uint8_t reason)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct wlan_objmgr_vdev *vdev;
	bool is_up;

	vdev = wlan_objmgr_get_vdev_by_id_from_pdev(pdev, vdev_id,
						    WLAN_MLME_NB_ID);
	if (!vdev)
		return status;

	is_up = QDF_IS_STATUS_SUCCESS(wlan_vdev_is_up(vdev));
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);

	if (requested_state != WLAN_ROAM_DEINIT && !is_up) {
		mlme_debug("ROAM: roam state change requested in disconnected state");
		return status;
	}

	switch (requested_state) {
	case WLAN_ROAM_DEINIT:
		status = cm_roam_switch_to_deinit(pdev, vdev_id, reason);
		break;
	case WLAN_ROAM_INIT:
		status = cm_roam_switch_to_init(pdev, vdev_id, reason);
		break;
	case WLAN_ROAM_RSO_ENABLED:
		status = cm_roam_switch_to_rso_enable(pdev, vdev_id, reason);
		break;
	case WLAN_ROAM_RSO_STOPPED:
		status = cm_roam_switch_to_rso_stop(pdev, vdev_id, reason);
		break;
	case WLAN_ROAMING_IN_PROG:
		status = cm_roam_switch_to_roam_start(pdev, vdev_id, reason);
		break;
	case WLAN_ROAM_SYNCH_IN_PROG:
		status = cm_roam_switch_to_roam_sync(pdev, vdev_id, reason);
		break;
	default:
		mlme_debug("ROAM: Invalid roam state %d", requested_state);
		break;
	}

	return status;
}
