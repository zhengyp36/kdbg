#define _KDBG_TRACE_ENABLE
#include <sys/ksym.h>
#include <sys/kdbg.h>
#include <sys/kdbg_trace.h>
#include <sys/kdbg_def_cmd.h>

#ifdef SPA_DEMO_ENABLE
#include "zfs_depend_impl.h"

#define FMT_PTR "0x%lx"
#define FMT_PTR_VAL(v) ((unsigned long)(v))

#define FMT_VDEV "vd{0x%lx:%llu%s%s}"
#define FMT_VDEV_VAL(v)							\
	(unsigned long)(v), (v)->vdev_id,				\
	((v)->vdev_path ? ":" : ""), (v)->vdev_path ?: ""

#define FMT_ZIO_TRACE "%x:%x"
#define FMT_ZIO_TRACE_VAL(zio)						\
	(zio ? zio_trace(zio)->flag - 1 : 0),				\
	(zio ? zio_trace(zio)->depth : 0)

#define VDEV_LABELS 4

#define	ZIO_FLAG_CANFAIL	(1ULL << 7)	/* must be first for INHERIT */
#define	ZIO_FLAG_SPECULATIVE	(1ULL << 8)
#define	ZIO_FLAG_CONFIG_WRITER	(1ULL << 9)
#define	ZIO_FLAG_DONT_RETRY	(1ULL << 10)
#define	ZIO_FLAG_DONT_CACHE	(1ULL << 11)
#define	ZIO_FLAG_NODATA		(1ULL << 12)
#define	ZIO_FLAG_INDUCE_DAMAGE	(1ULL << 13)
#define	ZIO_FLAG_IO_ALLOCATING	(1ULL << 14)

#define	VDEV_PAD_SIZE		(8 << 10)
/* 2 padding areas (vl_pad1 and vl_be) to skip */
#define	VDEV_SKIP_SIZE		VDEV_PAD_SIZE * 2
#define	VDEV_PHYS_SIZE		(112 << 10)
#define	VDEV_UBERBLOCK_RING	(128 << 10)

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif

typedef struct vdev_label {
	char		vl_pad1[VDEV_PAD_SIZE];			/*  8K */
	vdev_boot_envblock_t	vl_be;				/*  8K */
	vdev_phys_t	vl_vdev_phys;				/* 112K	*/
	char		vl_uberblock[VDEV_UBERBLOCK_RING];	/* 128K	*/
} vdev_label_t;						/* 256K total */

KFUN_IMPORT(zfs, int, spa_open,
    (const char *, spa_t **, const void *), 2);
#define spa_open KSYM_REF(zfs,spa_open,2)

KFUN_IMPORT(zfs, void, spa_close, (spa_t *, const void *));
#define spa_close KSYM_REF(zfs,spa_close)

KFUN_IMPORT(zfs, char *, nvpair_name, (const nvpair_t*));
#define nvpair_name KSYM_REF(zfs,nvpair_name)

KFUN_IMPORT(zfs, nvpair_t*, nvlist_next_nvpair, (nvlist_t *, const nvpair_t *));
#define nvlist_next_nvpair KSYM_REF(zfs,nvlist_next_nvpair)

KFUN_IMPORT(zfs, abd_t *, abd_alloc_linear, (size_t, boolean_t));
#define abd_alloc_linear KSYM_REF(zfs,abd_alloc_linear)

KFUN_IMPORT(zfs, void *, abd_to_buf, (abd_t *));
#define abd_to_buf KSYM_REF(zfs,abd_to_buf)

KFUN_IMPORT(zfs, void, abd_free, (abd_t *));
#define abd_free KSYM_REF(zfs,abd_free)

KFUN_IMPORT(zfs, zio_t*, zio_root,
	(spa_t *, void (*)(zio_t*), void *, zio_flag_t));
#define zio_root KSYM_REF(zfs,zio_root)

KFUN_IMPORT(zfs, int, zio_wait, (zio_t*));
#define zio_wait KSYM_REF(zfs,zio_wait)

KFUN_IMPORT(zfs, void, vdev_label_read,
	(zio_t *, vdev_t *, int, abd_t *, uint64_t,
	    uint64_t, void (*)(zio_t*), void *, int));
#define vdev_label_read KSYM_REF(zfs,vdev_label_read)

KFUN_IMPORT(zfs, int, nvlist_unpack,
	(char *, size_t, nvlist_t **, int));
#define nvlist_unpack KSYM_REF(zfs,nvlist_unpack)

KFUN_IMPORT(zfs, void, nvlist_free, (nvlist_t*));
#define nvlist_free KSYM_REF(zfs,nvlist_free)

KFUN_IMPORT(zfs, data_type_t, nvpair_type, (const nvpair_t*));
#define nvpair_type KSYM_REF(zfs,nvpair_type)

KVAR_IMPORT(zfs, vdev_ops_t, vdev_disk_ops);
#define vdev_disk_ops KSYM_REF(zfs,vdev_disk_ops)
// vdev_disk_ops.vdev_op_io_start : vdev_disk_io_start

static const char *zio_pipeline_name[] = {
	"(open)",
	"zio_read_bp_init",
	"zio_write_bp_init",
	"zio_free_bp_init",
	"zio_issue_async",
	"zio_write_compress",
	"zio_encrypt",
	"zio_checksum_generate",
	"zio_nop_write",
	"zio_ddt_read_start",
	"zio_ddt_read_done",
	"zio_ddt_write",
	"zio_ddt_free",
	"zio_gang_assemble",
	"zio_gang_issue",
	"zio_dva_throttle",
	"zio_dva_allocate",
	"zio_dva_free",
	"zio_dva_claim",
	"zio_ready",
	"zio_vdev_io_start",
	"zio_vdev_io_done",
	"zio_vdev_io_assess",
	"zio_checksum_verify",
	"zio_done"
};

static const char *
type2str(data_type_t type)
{
#define DEF_ENT(t) [t] = #t
	const char *tbl[] = {
		DEF_ENT(DATA_TYPE_UNKNOWN),
		DEF_ENT(DATA_TYPE_BOOLEAN),
		DEF_ENT(DATA_TYPE_BYTE),
		DEF_ENT(DATA_TYPE_INT16),
		DEF_ENT(DATA_TYPE_UINT16),
		DEF_ENT(DATA_TYPE_INT32),
		DEF_ENT(DATA_TYPE_UINT32),
		DEF_ENT(DATA_TYPE_INT64),
		DEF_ENT(DATA_TYPE_UINT64),
		DEF_ENT(DATA_TYPE_STRING),
		DEF_ENT(DATA_TYPE_BYTE_ARRAY),
		DEF_ENT(DATA_TYPE_INT16_ARRAY),
		DEF_ENT(DATA_TYPE_UINT16_ARRAY),
		DEF_ENT(DATA_TYPE_INT32_ARRAY),
		DEF_ENT(DATA_TYPE_UINT32_ARRAY),
		DEF_ENT(DATA_TYPE_INT64_ARRAY),
		DEF_ENT(DATA_TYPE_UINT64_ARRAY),
		DEF_ENT(DATA_TYPE_STRING_ARRAY),
		DEF_ENT(DATA_TYPE_HRTIME),
		DEF_ENT(DATA_TYPE_NVLIST),
		DEF_ENT(DATA_TYPE_NVLIST_ARRAY),
		DEF_ENT(DATA_TYPE_BOOLEAN_VALUE),
		DEF_ENT(DATA_TYPE_INT8),
		DEF_ENT(DATA_TYPE_UINT8),
		DEF_ENT(DATA_TYPE_BOOLEAN_ARRAY),
		DEF_ENT(DATA_TYPE_INT8_ARRAY),
		DEF_ENT(DATA_TYPE_UINT8_ARRAY)
	};
	return ((type > 0 && type < ARRAY_SIZE(tbl)) ? tbl[type] : tbl[0]);
#undef DEF_ENT
}

static inline const char *
zio_stage_ndx_to_name(int ndx)
{
	if (ndx >= 0 && ndx <= ARRAY_SIZE(zio_pipeline_name))
		return (zio_pipeline_name[ndx]);
	else
		return ("invalid");
}

typedef struct {
	uint32_t flag;
	uint32_t depth;
} zio_trace_detail_t;

static inline zio_trace_detail_t *
zio_trace(zio_t *zio)
{
	return (zio_trace_detail_t*)&zio->io_trace;
}

static void
zio_set_trace_by_pio(zio_t *zio, zio_t *pio)
{
	if (pio && zio_trace(pio)->flag) {
		zio_trace(zio)->flag = zio_trace(pio)->flag;
		zio_trace(zio)->depth = zio_trace(pio)->depth + 1;
	}
}

static void
zio_set_trace(zio_t *zio, uint32_t flag)
{
	zio_trace(zio)->flag = flag + 1;
	zio_trace(zio)->depth = 0;
}

static int
get_nvlist_keys(char *buffer, int size, const char *sep, nvlist_t *nvlist)
{
	char *pbuf = buffer;
	int remain = size;
	int cnt = 0;

	for (nvpair_t *elem = nvlist_next_nvpair(nvlist, NULL);
	    elem != NULL; elem = nvlist_next_nvpair(nvlist, elem)) {
		int r = snprintf(pbuf, remain, "%s%s",
		    (cnt ? sep : ""), nvpair_name(elem));
		if (r > 0 && r < remain) {
			pbuf += r;
			remain -= r;
		}
		cnt++;
	}
	return (cnt);
}

KFUN_IMPORT(zfs, int, nvpair_value_uint64, (nvpair_t *, uint64_t *));
#define nvpair_value_uint64 KSYM_REF(zfs,nvpair_value_uint64)

KFUN_IMPORT(zfs, int, nvpair_value_string, (nvpair_t *, char **));
#define nvpair_value_string KSYM_REF(zfs,nvpair_value_string)

KFUN_IMPORT(zfs, int, nvpair_value_nvlist, (nvpair_t *, nvlist_t **));
#define nvpair_value_nvlist KSYM_REF(zfs,nvpair_value_nvlist)

KFUN_IMPORT(zfs, int, nvpair_value_nvlist_array,
    (nvpair_t *nvp, nvlist_t ***val, uint_t *nelem));
#define nvpair_value_nvlist_array KSYM_REF(zfs,nvpair_value_nvlist_array)

static void dump_nvpair(drv_inst_t *, int indent, nvpair_t *);

static void
dump_nvlist(drv_inst_t *inst, int indent, nvlist_t *nvlist)
{
	for (nvpair_t *elem = nvlist_next_nvpair(nvlist, NULL);
	    elem != NULL; elem = nvlist_next_nvpair(nvlist, elem)) {
		dump_nvpair(inst, indent, elem);
	}
}

static void
dump_nvpair(drv_inst_t *inst, int indent, nvpair_t *elem)
{
	switch (nvpair_type(elem)) {
		case DATA_TYPE_UINT64: {
			uint64_t val;
			nvpair_value_uint64(elem, &val);
			kdbg_print(inst, "%*s%s=[u64]%llu\n",
			    indent, "", nvpair_name(elem), val);
			break;
		}

		case DATA_TYPE_STRING: {
			char *str = NULL;
			nvpair_value_string(elem, &str);
			kdbg_print(inst, "%*s%s=[str]%s\n",
			    indent, "", nvpair_name(elem), str ? str : "(null)");
			break;
		}

		case DATA_TYPE_NVLIST: {
			kdbg_print(inst, "%*s%s=[nvlist]{\n", indent, "",
			    nvpair_name(elem));
			nvlist_t *next_nvl = NULL;
			nvpair_value_nvlist(elem, &next_nvl);
			if (next_nvl) {
				dump_nvlist(inst, indent+4, next_nvl);
			}
			kdbg_print(inst, "%*s}\n", indent, "");
			break;
		}

		case DATA_TYPE_NVLIST_ARRAY: {
			nvlist_t **nvlist_array_value = NULL;
			int count = 0;
			(void) nvpair_value_nvlist_array(elem,
			    &nvlist_array_value, &count);
			for (int i = 0; i < count; i++) {
				(void) kdbg_print(inst, "%*s%s[%u]:\n",
				    indent, "", nvpair_name(elem), i);
				dump_nvlist(inst, indent + 4,
				    nvlist_array_value[i]);
			}
			break;
		}
		default: {
			kdbg_print(inst, "%*s%s=%d[%s]\n",
			    indent, "", nvpair_name(elem), nvpair_type(elem),
			    type2str(nvpair_type(elem)));
			break;
		}
	}
}

static void
dump_single_label(drv_inst_t *inst, nvlist_t *nvlist)
{
	dump_nvlist(inst, 0, nvlist);
}

static void 
dump_hex(char *buffer, size_t size, const uint64_t *data, int cnt)
{
	char *pbuf = buffer;
	int remain = size;

	for (int i = 0; i < cnt; i++) {
		int r = snprintf(pbuf, remain, "%016llx", data[i]);
		if (r > 0 && r < remain) {
			pbuf += r;
			remain -= r;
		}
	}
}

static void
dump_vdev_label_inner(drv_inst_t *inst, zio_t *zio, int l,
    vdev_phys_t *vp[], nvlist_t *label, spa_t *spa, int flags, vdev_t *vd,
    abd_t *vp_abd[])
{
	static int callcnt = 0;
	int callndx = __sync_add_and_fetch(&callcnt, 1);

	char keys[256];
	int keycnt = get_nvlist_keys(keys, sizeof(keys), " ", label);

	#define DUMP_U64_CNT 16
	char dump[DUMP_U64_CNT * 8 * 2 + 1];
	dump_hex(dump, sizeof(dump), (uint64_t*)vp[l]->vp_nvlist, DUMP_U64_CNT);

	if (!inst)
		kdbg_log("READ_LABEL: "FMT_VDEV", ndx{%d:%d}, label="FMT_PTR", "
		    "keys={%d:%s},hex[%d]{%s}", FMT_VDEV_VAL(vd), callndx, l,
		    FMT_PTR_VAL(label), keycnt, keys, DUMP_U64_CNT, dump);
	else
		kdbg_print(inst, "READ_LABEL: "FMT_VDEV", ndx{%d:%d}, label="
		    FMT_PTR", keys={%d:%s}, ,hex[%d]{%s}\n", FMT_VDEV_VAL(vd),
		    callndx, l, FMT_PTR_VAL(label), keycnt, keys,
		    DUMP_U64_CNT, dump);
}

static void
get_labels(vdev_t *vd, nvlist_t *labels[], int cnt)
{
	spa_t *spa = vd->vdev_spa;

	abd_t *vp_abd[VDEV_LABELS];
	vdev_phys_t *vp[VDEV_LABELS];
	zio_t *zio[VDEV_LABELS];

	int flags = ZIO_FLAG_CONFIG_WRITER | ZIO_FLAG_CANFAIL |
	    ZIO_FLAG_SPECULATIVE;

	for (int i = 0; i < cnt; i++)
		labels[i] = NULL;

	for (int l = 0; l < VDEV_LABELS; l++) {
		vp_abd[l] = abd_alloc_linear(sizeof (vdev_phys_t), B_TRUE);
		vp[l] = abd_to_buf(vp_abd[l]);
	}

	for (int l = 0; l < VDEV_LABELS; l++) {
		zio[l] = zio_root(spa, NULL, NULL, flags);
		zio_set_trace(zio[l], l);
		vdev_label_read(zio[l], vd, l, vp_abd[l],
		    offsetof(vdev_label_t, vl_vdev_phys), sizeof (vdev_phys_t),
		    NULL, NULL, flags);
	}

	for (int l = 0; l < VDEV_LABELS; l++) {
		nvlist_t *label = NULL;
		if (zio_wait(zio[l]) == 0 &&
		    nvlist_unpack(vp[l]->vp_nvlist, sizeof(vp[l]->vp_nvlist),
		    &label, 0) == 0) {
		}
		if (labels && l < cnt) {
			labels[l] = label;
			label = NULL;
		} else if (label != NULL) {
			nvlist_free(label);
			label = NULL;
		}
	}

	for (int l = 0; l < VDEV_LABELS; l++) {
		abd_free(vp_abd[l]);
	}
}

static void
put_labels(nvlist_t *labels[], int cnt)
{
	for (int i = 0; i < cnt; i++) {
		if (labels[i]) {
			nvlist_free(labels[i]);
			labels[i] = NULL;
		}
	}
}

static void
read_labels(drv_inst_t *inst, vdev_t *vd)
{
	spa_t *spa = vd->vdev_spa;

	abd_t *vp_abd[VDEV_LABELS];
	vdev_phys_t *vp[VDEV_LABELS];
	zio_t *zio[VDEV_LABELS];

	int flags = ZIO_FLAG_CONFIG_WRITER | ZIO_FLAG_CANFAIL |
	    ZIO_FLAG_SPECULATIVE;

	for (int l = 0; l < VDEV_LABELS; l++) {
		vp_abd[l] = abd_alloc_linear(sizeof (vdev_phys_t), B_TRUE);
		vp[l] = abd_to_buf(vp_abd[l]);
	}

	for (int l = 0; l < VDEV_LABELS; l++) {
		zio[l] = zio_root(spa, NULL, NULL, flags);
		zio_set_trace(zio[l], l);
		vdev_label_read(zio[l], vd, l, vp_abd[l],
		    offsetof(vdev_label_t, vl_vdev_phys), sizeof (vdev_phys_t),
		    NULL, NULL, flags);
	}

	for (int l = 0; l < VDEV_LABELS; l++) {
		nvlist_t *label = NULL;
		if (zio_wait(zio[l]) == 0 &&
		    nvlist_unpack(vp[l]->vp_nvlist, sizeof(vp[l]->vp_nvlist),
		    &label, 0) == 0) {
			dump_vdev_label_inner(inst,
			    zio[l], l, vp, label, spa, flags, vd, vp_abd);
		}
		if (label != NULL) {
			nvlist_free(label);
			label = NULL;
		}
	}

	for (int l = 0; l < VDEV_LABELS; l++) {
		abd_free(vp_abd[l]);
	}
}

static vdev_t *
get_min_vdev(spa_t *spa)
{
	vdev_t *vd = spa->spa_root_vdev;
	while (vd && vd->vdev_children > 0)
		vd = vd->vdev_child[0];
	return (vd);
}

static void
print_tabs(drv_inst_t *inst, int tabs)
{
	while (tabs-- > 0)
		kdbg_print(inst, "\t");
}

static void
dump_vdev_(drv_inst_t *inst, vdev_t *vd, int depth)
{
	if (!vd)
		return;

	print_tabs(inst, depth);
	kdbg_print(inst, "|"FMT_VDEV"\n", FMT_VDEV_VAL(vd));
	for (int i = 0; i < vd->vdev_children; i++)
		dump_vdev_(inst, vd->vdev_child[i], depth+1);
}

static void
dump_vdev(drv_inst_t *inst, vdev_t *vd)
{
	dump_vdev_(inst, vd, 0);
}

static void
zfs_label_main(drv_inst_t *inst, int argc, char *argv[])
{
	if (argc < 2) {
		kdbg_print(inst, "Usage: %s <poolname>\n", argv[0]);
		return;
	}

	spa_t *spa = NULL;
	const char *poolname = argv[1];

	int rc = spa_open(poolname, &spa, FTAG);
	if (rc) {
		kdbg_print(inst, "Open %s failure\n", poolname);
		return;
	}

	if ((argc == 2) || (argc == 3 && !strcmp(argv[2],"read"))) {
		kdbg_print(inst, "spa="FMT_PTR"\n", FMT_PTR_VAL(spa));
		dump_vdev(inst, spa->spa_root_vdev);
		kdbg_print(inst, "min:vdev="FMT_PTR"\n",
		    FMT_PTR_VAL(get_min_vdev(spa)));
		read_labels(inst, get_min_vdev(spa));
	} else if (!strcmp(argv[2], "dump")) {
		nvlist_t *labels[VDEV_LABELS];
		vdev_t *vd = get_min_vdev(spa);
		if (vd) {
			get_labels(vd, labels, VDEV_LABELS);
			for (int i = 0; i < VDEV_LABELS; i++) {
				if (labels[i]) {
					kdbg_print(inst, "LABEL[%d]\n", i);
					dump_single_label(inst, labels[i]);
				}
			}
			put_labels(labels, VDEV_LABELS);
		} else {
			kdbg_print(inst, "Failed to get vdev\n");
		}
	} else {
		kdbg_print(inst, "invalid cmd(%s)\n", argv[2]);
	}
	spa_close(spa, FTAG);
}
#endif // SPA_DEMO_ENABLE

KDBG_CMD_DEF(zfslabel, "<poolname> <read|dump>", drv_inst_t *inst, int argc, char *argv[])
{
#ifdef SPA_DEMO_ENABLE
	zfs_label_main(inst, argc, argv);
#else
	kdbg_print(inst, "Please set environ varible spa_demo_enable=true "
	    "and recompile kdbg.ko\n");
#endif // SPA_DEMO_ENABLE
	return (0);
}

#ifdef SPA_DEMO_ENABLE
#include <sys/kdbg_trace_impl.h>
KDBG_TRACE_DEFINE(zfs, ZIO_CREATE, zio_t *zio, zio_t *pio)
{
	zio_set_trace_by_pio(zio, pio);
}

KDBG_TRACE_DEFINE(zfs, ZIO_CUT_IN_LINE, zio_t *zio, enum zio_stage stage,
    boolean_t cut, int trace_ndx)
{
}

KDBG_TRACE_DEFINE(zfs, ZIO_BEFORE_EXEC, zio_t *zio, int stage_ndx)
{
	if (zio && zio_trace(zio)->flag) {
		kdbg_log("ZIO-EXEC-1: zio="FMT_PTR",trace="FMT_ZIO_TRACE","
		    "stage=%x,pipeline=%x,stage_ndx=%d[%s]", FMT_PTR_VAL(zio),
		    FMT_ZIO_TRACE_VAL(zio), zio->io_stage,
		    zio->io_pipeline_trace, stage_ndx,
		    zio_stage_ndx_to_name(stage_ndx));
	}
}

// last_zio: maybe freed
KDBG_TRACE_DEFINE(zfs, ZIO_AFTER_EXEC, zio_t *zio,
    zio_t *last_zio, int stage_ndx)
{
}

KDBG_TRACE_DEFINE(zfs, VDEV_LABEL_READ_CONFIG, zio_t *zio, int l, spa_t *spa,
    int flags, vdev_t *vd, abd_t *vp_abd[])
{
}

KDBG_TRACE_DEFINE(zfs, VDEV_LABEL_READ, zio_t *zio, zio_t *cio, vdev_t *vd,
    int l, abd_t *buf, uint64_t offset, uint64_t size, void *done,
    void *private, int flags, uint64_t label_off)
{
	kdbg_log("READ_LABEL_PHYS: "FMT_VDEV", ndx=%d, offset=%llx|%llx, "
	    "size=%llx", FMT_VDEV_VAL(vd), l, offset, label_off, size);
}

/* zio is freed */
KDBG_TRACE_DEFINE(zfs, VDEV_LABEL_READ_CONFIG_DONE, zio_t *zio, int l,
    vdev_phys_t *vp[], nvlist_t *label, spa_t *spa, int flags, vdev_t *vd,
    abd_t *vp_abd[])
{
	dump_vdev_label_inner(NULL, zio, l, vp, label, spa, flags, vd, vp_abd);
}

KDBG_TRACE_DEFINE(zfs, VDEV_LABEL_WRITE, zio_t *zio, zio_t *cio, vdev_t *vd,
    int l, abd_t *buf, uint64_t offset, uint64_t size, void *done,
    void *private, int flags, uint64_t label_off)
{
}
#endif // SPA_DEMO_ENABLE
