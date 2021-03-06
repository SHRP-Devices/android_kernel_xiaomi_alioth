/*
 * Copyright (c) 2017-2019 The Linux Foundation. All rights reserved.
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

#include <wlan_objmgr_pdev_obj.h>
#include <dp_txrx.h>
#include "dp_types.h"
#include <dp_internal.h>
#include <cdp_txrx_cmn.h>
#include <cdp_txrx_misc.h>
#include "dp_tx_desc.h"
#include "dp_rx.h"


QDF_STATUS dp_txrx_init(ol_txrx_soc_handle soc, uint8_t pdev_id,
			struct dp_txrx_config *config)
{
	struct dp_txrx_handle *dp_ext_hdl;
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;
	uint8_t num_dp_rx_threads;
	struct dp_pdev *pdev;

	if (qdf_unlikely(!soc)) {
		dp_err("soc is NULL");
		return 0;
	}

	pdev = dp_get_pdev_from_soc_pdev_id_wifi3(cdp_soc_t_to_dp_soc(soc),
						  pdev_id);
	if (!pdev) {
		dp_err("pdev is NULL");
		return 0;
	}

	dp_ext_hdl = qdf_mem_malloc(sizeof(*dp_ext_hdl));
	if (!dp_ext_hdl) {
		QDF_ASSERT(0);
		return QDF_STATUS_E_NOMEM;
	}

	dp_info("dp_txrx_handle allocated");
	dp_ext_hdl->soc = soc;
	dp_ext_hdl->pdev = dp_pdev_to_cdp_pdev(pdev);
	cdp_soc_set_dp_txrx_handle(soc, dp_ext_hdl);
	qdf_mem_copy(&dp_ext_hdl->config, config, sizeof(*config));
	dp_ext_hdl->rx_tm_hdl.txrx_handle_cmn =
				dp_txrx_get_cmn_hdl_frm_ext_hdl(dp_ext_hdl);

	num_dp_rx_threads = cdp_get_num_rx_contexts(soc);

	if (dp_ext_hdl->config.enable_rx_threads) {
		qdf_status = dp_rx_tm_init(&dp_ext_hdl->rx_tm_hdl,
					   num_dp_rx_threads);
	}

	return qdf_status;
}

QDF_STATUS dp_txrx_deinit(ol_txrx_soc_handle soc)
{
	struct dp_txrx_handle *dp_ext_hdl;

	if (!soc)
		return QDF_STATUS_E_INVAL;

	dp_ext_hdl = cdp_soc_get_dp_txrx_handle(soc);
	if (!dp_ext_hdl)
		return QDF_STATUS_E_FAULT;

	if (dp_ext_hdl->config.enable_rx_threads)
		dp_rx_tm_deinit(&dp_ext_hdl->rx_tm_hdl);

	qdf_mem_free(dp_ext_hdl);
	dp_info("dp_txrx_handle_t de-allocated");

	cdp_soc_set_dp_txrx_handle(soc, NULL);

	return QDF_STATUS_SUCCESS;
}

#ifdef DP_MEM_PRE_ALLOC

/* Num elements in REO ring */
#define REO_DST_RING_SIZE 1024

/* Num elements in TCL Data ring */
#define TCL_DATA_RING_SIZE 3072

/* Num elements in WBM2SW ring */
#define WBM2SW_RELEASE_RING_SIZE 4096

/* Num elements in WBM Idle Link */
#define WBM_IDLE_LINK_RING_SIZE (32 * 1024)

/* Num TX desc in TX desc pool */
#define DP_TX_DESC_POOL_SIZE 4096


/**
 * struct dp_consistent_prealloc - element representing DP pre-alloc memory
 * @ring_type: HAL ring type
 * @size: size of pre-alloc memory
 * @in_use: whether this element is in use (occupied)
 * @va_unaligned: Unaligned virtual address
 * @va_aligned: aligned virtual address.
 * @pa_unaligned: Unaligned physical address.
 * @pa_aligned: Aligned physical address.
 */

struct dp_consistent_prealloc {
	enum hal_ring_type ring_type;
	uint32_t size;
	uint8_t in_use;
	void *va_unaligned;
	void *va_aligned;
	qdf_dma_addr_t pa_unaligned;
	qdf_dma_addr_t pa_aligned;
};

/**
 * struct dp_multi_page_prealloc -  element representing DP pre-alloc multiple
				    pages memory
 * @desc_type: source descriptor type for memory allocation
 * @element_size: single element size
 * @element_num: total number of elements should be allocated
 * @in_use: whether this element is in use (occupied)
 * @cacheable: coherent memory or cacheable memory
 * @pages: multi page information storage
 */
struct dp_multi_page_prealloc {
	enum dp_desc_type desc_type;
	size_t element_size;
	uint16_t element_num;
	bool in_use;
	bool cacheable;
	struct qdf_mem_multi_page_t pages;
};

static struct  dp_consistent_prealloc g_dp_consistent_allocs[] = {
	/* 5 REO DST rings */
	{REO_DST, (sizeof(struct reo_destination_ring)) * REO_DST_RING_SIZE, 0, NULL, NULL, 0, 0},
	{REO_DST, (sizeof(struct reo_destination_ring)) * REO_DST_RING_SIZE, 0, NULL, NULL, 0, 0},
	{REO_DST, (sizeof(struct reo_destination_ring)) * REO_DST_RING_SIZE, 0, NULL, NULL, 0, 0},
	{REO_DST, (sizeof(struct reo_destination_ring)) * REO_DST_RING_SIZE, 0, NULL, NULL, 0, 0},
	{REO_DST, (sizeof(struct reo_destination_ring)) * REO_DST_RING_SIZE, 0, NULL, NULL, 0, 0},
	/* 3 TCL data rings */
	{TCL_DATA, (sizeof(struct tlv_32_hdr) + sizeof(struct tcl_data_cmd)) * TCL_DATA_RING_SIZE, 0, NULL, NULL, 0, 0},
	{TCL_DATA, (sizeof(struct tlv_32_hdr) + sizeof(struct tcl_data_cmd)) * TCL_DATA_RING_SIZE, 0, NULL, NULL, 0, 0},
	{TCL_DATA, (sizeof(struct tlv_32_hdr) + sizeof(struct tcl_data_cmd)) * TCL_DATA_RING_SIZE, 0, NULL, NULL, 0, 0},
	/* 4 WBM2SW rings */
	{WBM2SW_RELEASE, (sizeof(struct wbm_release_ring)) * WBM2SW_RELEASE_RING_SIZE, 0, NULL, NULL, 0, 0},
	{WBM2SW_RELEASE, (sizeof(struct wbm_release_ring)) * WBM2SW_RELEASE_RING_SIZE, 0, NULL, NULL, 0, 0},
	{WBM2SW_RELEASE, (sizeof(struct wbm_release_ring)) * WBM2SW_RELEASE_RING_SIZE, 0, NULL, NULL, 0, 0},
	{WBM2SW_RELEASE, (sizeof(struct wbm_release_ring)) * WBM2SW_RELEASE_RING_SIZE, 0, NULL, 0, 0},
	/* SW2WBM link descriptor return ring */
	{SW2WBM_RELEASE, (sizeof(struct wbm_release_ring)) * WLAN_CFG_WBM_RELEASE_RING_SIZE, 0, NULL, 0, 0},
	/* 1 WBM idle link desc ring */
	{WBM_IDLE_LINK, (sizeof(struct wbm_link_descriptor_ring)) * WBM_IDLE_LINK_RING_SIZE, 0, NULL, NULL, 0, 0},
	/* 2 RXDMA DST ERR rings */
	{RXDMA_DST, (sizeof(struct reo_entrance_ring)) * WLAN_CFG_RXDMA_ERR_DST_RING_SIZE, 0, NULL, NULL, 0, 0},
	{RXDMA_DST, (sizeof(struct reo_entrance_ring)) * WLAN_CFG_RXDMA_ERR_DST_RING_SIZE, 0, NULL, NULL, 0, 0},
	/* REFILL ring 0 */
	{RXDMA_BUF, (sizeof(struct wbm_buffer_ring)) * WLAN_CFG_RXDMA_REFILL_RING_SIZE, 0, NULL, NULL, 0, 0},

};

/* Number of HW link descriptors needed (rounded to power of 2) */
#define NUM_HW_LINK_DESCS (32 * 1024)

/* Size in bytes of HW LINK DESC */
#define HW_LINK_DESC_SIZE 128

/* Size in bytes of TX Desc (rounded to power of 2) */
#define TX_DESC_SIZE 128

/* Size in bytes of TX TSO Desc (rounded to power of 2) */
#define TX_TSO_DESC_SIZE 256

/* Size in bytes of TX TSO Num Seg Desc (rounded to power of 2) */
#define TX_TSO_NUM_SEG_DESC_SIZE 16


#define NON_CACHEABLE 0
#define CACHEABLE 1

static struct  dp_multi_page_prealloc g_dp_multi_page_allocs[] = {
	/* 4 TX DESC pools */
	{DP_TX_DESC_TYPE, TX_DESC_SIZE, DP_TX_DESC_POOL_SIZE, 0, CACHEABLE, { 0 }},
	{DP_TX_DESC_TYPE, TX_DESC_SIZE, DP_TX_DESC_POOL_SIZE, 0, CACHEABLE, { 0 }},
	{DP_TX_DESC_TYPE, TX_DESC_SIZE, DP_TX_DESC_POOL_SIZE, 0, CACHEABLE, { 0 }},
	{DP_TX_DESC_TYPE, TX_DESC_SIZE, DP_TX_DESC_POOL_SIZE, 0, CACHEABLE, { 0 }},

	/* 4 Tx EXT DESC NON Cacheable pools */
	{DP_TX_EXT_DESC_TYPE, HAL_TX_EXT_DESC_WITH_META_DATA, DP_TX_DESC_POOL_SIZE, 0, NON_CACHEABLE, { 0 }},
	{DP_TX_EXT_DESC_TYPE, HAL_TX_EXT_DESC_WITH_META_DATA, DP_TX_DESC_POOL_SIZE, 0, NON_CACHEABLE, { 0 }},
	{DP_TX_EXT_DESC_TYPE, HAL_TX_EXT_DESC_WITH_META_DATA, DP_TX_DESC_POOL_SIZE, 0, NON_CACHEABLE, { 0 }},
	{DP_TX_EXT_DESC_TYPE, HAL_TX_EXT_DESC_WITH_META_DATA, DP_TX_DESC_POOL_SIZE, 0, NON_CACHEABLE, { 0 }},

	/* 4 Tx EXT DESC Link Cacheable pools */
	{DP_TX_EXT_DESC_LINK_TYPE, sizeof(struct dp_tx_ext_desc_elem_s), DP_TX_DESC_POOL_SIZE, 0, CACHEABLE, { 0 }},
	{DP_TX_EXT_DESC_LINK_TYPE, sizeof(struct dp_tx_ext_desc_elem_s), DP_TX_DESC_POOL_SIZE, 0, CACHEABLE, { 0 }},
	{DP_TX_EXT_DESC_LINK_TYPE, sizeof(struct dp_tx_ext_desc_elem_s), DP_TX_DESC_POOL_SIZE, 0, CACHEABLE, { 0 }},
	{DP_TX_EXT_DESC_LINK_TYPE, sizeof(struct dp_tx_ext_desc_elem_s), DP_TX_DESC_POOL_SIZE, 0, CACHEABLE, { 0 }},

	/* 4 TX TSO DESC pools */
	{DP_TX_TSO_DESC_TYPE, TX_TSO_DESC_SIZE, DP_TX_DESC_POOL_SIZE, 0, CACHEABLE, { 0 }},
	{DP_TX_TSO_DESC_TYPE, TX_TSO_DESC_SIZE, DP_TX_DESC_POOL_SIZE, 0, CACHEABLE, { 0 }},
	{DP_TX_TSO_DESC_TYPE, TX_TSO_DESC_SIZE, DP_TX_DESC_POOL_SIZE, 0, CACHEABLE, { 0 }},
	{DP_TX_TSO_DESC_TYPE, TX_TSO_DESC_SIZE, DP_TX_DESC_POOL_SIZE, 0, CACHEABLE, { 0 }},



	/* 4 TX TSO NUM SEG DESC pools */
	{DP_TX_TSO_NUM_SEG_TYPE, TX_TSO_NUM_SEG_DESC_SIZE, DP_TX_DESC_POOL_SIZE, 0, CACHEABLE, { 0 }},
	{DP_TX_TSO_NUM_SEG_TYPE, TX_TSO_NUM_SEG_DESC_SIZE, DP_TX_DESC_POOL_SIZE, 0, CACHEABLE, { 0 }},
	{DP_TX_TSO_NUM_SEG_TYPE, TX_TSO_NUM_SEG_DESC_SIZE, DP_TX_DESC_POOL_SIZE, 0, CACHEABLE, { 0 }},
	{DP_TX_TSO_NUM_SEG_TYPE, TX_TSO_NUM_SEG_DESC_SIZE, DP_TX_DESC_POOL_SIZE, 0, CACHEABLE, { 0 }},

	/* DP RX DESCs pools */
	{DP_RX_DESC_BUF_TYPE, sizeof(union dp_rx_desc_list_elem_t),
	 WLAN_CFG_RX_SW_DESC_WEIGHT_SIZE * WLAN_CFG_RXDMA_REFILL_RING_SIZE, 0, CACHEABLE, { 0 }},

	/* DP HW Link DESCs pools */
	{DP_HW_LINK_DESC_TYPE, HW_LINK_DESC_SIZE, NUM_HW_LINK_DESCS, 0, NON_CACHEABLE, { 0 }},

};

void dp_prealloc_deinit(void)
{
	int i;
	struct dp_consistent_prealloc *p;
	struct dp_multi_page_prealloc *mp;
	qdf_device_t qdf_ctx = cds_get_context(QDF_MODULE_ID_QDF_DEVICE);

	for (i = 0; i < QDF_ARRAY_SIZE(g_dp_consistent_allocs); i++) {
		p = &g_dp_consistent_allocs[i];

		if (p->in_use)
			dp_info("i %d: consistent_mem in use while free", i);

		if (p->va_aligned) {
			dp_info("i %d: va aligned %pK pa aligned %x size %d",
				i, p->va_aligned, p->pa_aligned, p->size);
			qdf_mem_free_consistent(qdf_ctx, qdf_ctx->dev,
						p->size,
						p->va_unaligned,
						p->pa_unaligned, 0);
			qdf_mem_zero(p, sizeof(*p));
		}
	}

	for (i = 0; i < QDF_ARRAY_SIZE(g_dp_multi_page_allocs); i++) {
		mp = &g_dp_multi_page_allocs[i];

		if (mp->in_use)
			dp_info("i %d: multi-page mem in use while free", i);

		if (mp->pages.num_pages) {
			dp_info("i %d: type %d cacheable_pages %pK dma_pages %pK num_pages %d",
				i, mp->desc_type,
				mp->pages.cacheable_pages,
				mp->pages.dma_pages,
				mp->pages.num_pages);
			qdf_mem_multi_pages_free(qdf_ctx, &mp->pages,
						 0, mp->cacheable);
			qdf_mem_zero(mp, sizeof(*mp));
		}
	}
}

QDF_STATUS dp_prealloc_init(void)
{
	int i;
	struct dp_consistent_prealloc *p;
	struct dp_multi_page_prealloc *mp;
	qdf_device_t qdf_ctx = cds_get_context(QDF_MODULE_ID_QDF_DEVICE);

	if (!qdf_ctx) {
		dp_err("qdf_ctx is NULL");
		QDF_BUG(0);
		return QDF_STATUS_E_FAILURE;
	}

	for (i = 0; i < QDF_ARRAY_SIZE(g_dp_consistent_allocs); i++) {
		p = &g_dp_consistent_allocs[i];
		p->in_use = 0;
		p->va_aligned =
			qdf_aligned_mem_alloc_consistent(qdf_ctx,
							 &p->size,
							 &p->va_unaligned,
							 &p->pa_unaligned,
							 &p->pa_aligned,
							 DP_RING_BASE_ALIGN);
		if (!p->va_unaligned) {
			dp_warn("i %d: unable to preallocate %d bytes memory!",
				i, p->size);
			break;
		} else {
			dp_info("i %d: va aligned %pK pa aligned %x size %d", i,
				p->va_aligned, p->pa_aligned, p->size);
		}
	}

	if (i != QDF_ARRAY_SIZE(g_dp_consistent_allocs)) {
		dp_info("unable to allocate consistent memory!");
		goto deinit;
	}

	for (i = 0; i < QDF_ARRAY_SIZE(g_dp_multi_page_allocs); i++) {
		mp = &g_dp_multi_page_allocs[i];
		mp->in_use = false;
		qdf_mem_multi_pages_alloc(qdf_ctx, &mp->pages,
					  mp->element_size,
					  mp->element_num,
					  0, mp->cacheable);
		if (!mp->pages.num_pages) {
			dp_warn("i %d: preallocate %d bytes multi-pages failed!",
				i, mp->element_size * mp->element_num);
			break;
		} else {
			mp->pages.is_mem_prealloc = true;
			dp_info("i %d: cacheable_pages %pK dma_pages %pK num_pages %d",
				i, mp->pages.cacheable_pages,
				mp->pages.dma_pages,
				mp->pages.num_pages);
		}
	}

	if (i != QDF_ARRAY_SIZE(g_dp_multi_page_allocs)) {
		dp_info("unable to allocate multi-pages memory!");
		goto deinit;
	}

	return QDF_STATUS_SUCCESS;
deinit:
	dp_prealloc_deinit();
	return QDF_STATUS_E_FAILURE;
}

void *dp_prealloc_get_coherent(uint32_t *size, void **base_vaddr_unaligned,
			       qdf_dma_addr_t *paddr_unaligned,
			       qdf_dma_addr_t *paddr_aligned,
			       uint32_t align,
			       uint32_t ring_type)
{
	int i;
	struct dp_consistent_prealloc *p;
	void *va_aligned = NULL;

	for (i = 0; i < QDF_ARRAY_SIZE(g_dp_consistent_allocs); i++) {
		p = &g_dp_consistent_allocs[i];
		if (p->ring_type == ring_type && !p->in_use &&
		    p->va_unaligned && *size <= p->size) {
			p->in_use = 1;
			*base_vaddr_unaligned = p->va_unaligned;
			*paddr_unaligned = p->pa_unaligned;
			*paddr_aligned = p->pa_aligned;
			va_aligned = p->va_aligned;
			*size = p->size;
			dp_info("index %i -> ring type %s va-aligned %pK", i,
				dp_srng_get_str_from_hal_ring_type(ring_type),
				va_aligned);
			break;
		}
	}

	return va_aligned;
}

void dp_prealloc_put_coherent(qdf_size_t size, void *vaddr_unligned,
			      qdf_dma_addr_t paddr)
{
	int i;
	struct dp_consistent_prealloc *p;

	for (i = 0; i < QDF_ARRAY_SIZE(g_dp_consistent_allocs); i++) {
		p = &g_dp_consistent_allocs[i];
		if (p->va_unaligned == vaddr_unligned) {
			dp_info("index %d, returned", i);
			p->in_use = 0;
			qdf_mem_zero(p->va_unaligned, p->size);
			break;
		}
	}

	if (i == QDF_ARRAY_SIZE(g_dp_consistent_allocs))
		dp_err("unable to find vaddr %pK", vaddr_unligned);
}

void dp_prealloc_get_multi_pages(uint32_t desc_type,
				 size_t element_size,
				 uint16_t element_num,
				 struct qdf_mem_multi_page_t *pages,
				 bool cacheable)
{
	int i;
	struct dp_multi_page_prealloc *mp;

	for (i = 0; i < QDF_ARRAY_SIZE(g_dp_multi_page_allocs); i++) {
		mp = &g_dp_multi_page_allocs[i];

		if (desc_type == mp->desc_type && !mp->in_use &&
		    mp->pages.num_pages && element_size == mp->element_size &&
		    element_num <= mp->element_num) {
			mp->in_use = true;
			*pages = mp->pages;

			dp_info("i %d: desc_type %d cacheable_pages %pK"
				"dma_pages %pK num_pages %d",
				i, desc_type,
				mp->pages.cacheable_pages,
				mp->pages.dma_pages,
				mp->pages.num_pages);
			break;
		}
	}
}

void dp_prealloc_put_multi_pages(uint32_t desc_type,
				 struct qdf_mem_multi_page_t *pages)
{
	int i;
	struct dp_multi_page_prealloc *mp;
	bool mp_found = false;

	for (i = 0; i < QDF_ARRAY_SIZE(g_dp_multi_page_allocs); i++) {
		mp = &g_dp_multi_page_allocs[i];

		if (desc_type == mp->desc_type) {
			/* compare different address by cacheable flag */
			mp_found = mp->cacheable ?
				(mp->pages.cacheable_pages ==
				 pages->cacheable_pages) :
				(mp->pages.dma_pages == pages->dma_pages);
			/* find it, put back to prealloc pool */
			if (mp_found) {
				dp_info("i %d: desc_type %d returned",
					i, desc_type);
				mp->in_use = false;
				/* is page memory zero needed? */
				break;
			}
		}
	}

	if (!mp_found)
		dp_warn("Not prealloc pages %pK desc_type %d"
			"cacheable_pages %pK dma_pages %pK",
			pages,
			desc_type,
			pages->cacheable_pages,
			pages->dma_pages);
}
#endif
