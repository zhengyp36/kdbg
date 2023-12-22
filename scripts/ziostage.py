#!/usr/bin/python

import os
import sys

class ZioStage(object):
    def __init__(self):
        self.stageInit()
    
    def stageBase(self, name, value, func):
        self.stages[name] = value
        self.values[value] = name
        self.funcs [value] = ':' + func
    
    def stageBias(self, bias, table):
        assert(bias not in self.stages)
        self.stages[bias] = self.sumStages(table)
    
    def sumStages(self, table):
        r = 0
        for stage in table:
            r |= self.stages[stage]
        return r
    
    def detailStage(self, detail):
        r,s = [],self.sumStages(detail.replace('|',' ').split())
        for i in range(64):
            mask = 1 << i
            if s & mask:
                r.append(self.values[mask]+self.funcs[mask])
        return r
    
    def stageInit(self):
        self.stages = {}
        self.values = {}
        self.funcs  = {}
        
        self.stageBase('ZIO_STAGE_OPEN',              1 <<  0, ''                     )   # RWFCI
        self.stageBase('ZIO_STAGE_READ_BP_INIT',      1 <<  1, 'zio_read_bp_init'     )   # R----
        self.stageBase('ZIO_STAGE_WRITE_BP_INIT',     1 <<  2, 'zio_write_bp_init'    )   # -W---
        self.stageBase('ZIO_STAGE_FREE_BP_INIT',      1 <<  3, 'zio_free_bp_init'     )   # --F--
        self.stageBase('ZIO_STAGE_ISSUE_ASYNC',       1 <<  4, 'zio_issue_async'      )   # RWF--
        self.stageBase('ZIO_STAGE_WRITE_COMPRESS',    1 <<  5, 'zio_write_compress'   )   # -W---
        self.stageBase('ZIO_STAGE_ENCRYPT',           1 <<  6, 'zio_encrypt'          )   # -W---
        self.stageBase('ZIO_STAGE_CHECKSUM_GENERATE', 1 <<  7, 'zio_checksum_generate')   # -W---
        self.stageBase('ZIO_STAGE_NOP_WRITE',         1 <<  8, 'zio_nop_write'        )   # -W---
        self.stageBase('ZIO_STAGE_DDT_READ_START',    1 <<  9, 'zio_ddt_read_start'   )   # R----
        self.stageBase('ZIO_STAGE_DDT_READ_DONE',     1 << 10, 'zio_ddt_read_done'    )   # R----
        self.stageBase('ZIO_STAGE_DDT_WRITE',         1 << 11, 'zio_ddt_write'        )   # -W---
        self.stageBase('ZIO_STAGE_DDT_FREE',          1 << 12, 'zio_ddt_free'         )   # --F--
        self.stageBase('ZIO_STAGE_GANG_ASSEMBLE',     1 << 13, 'zio_gang_assemble'    )   # RWFC-
        self.stageBase('ZIO_STAGE_GANG_ISSUE',        1 << 14, 'zio_gang_issue'       )   # RWFC-
        self.stageBase('ZIO_STAGE_DVA_THROTTLE',      1 << 15, 'zio_dva_throttle'     )   # -W---
        self.stageBase('ZIO_STAGE_DVA_ALLOCATE',      1 << 16, 'zio_dva_allocate'     )   # -W---
        self.stageBase('ZIO_STAGE_DVA_FREE',          1 << 17, 'zio_dva_free'         )   # --F--
        self.stageBase('ZIO_STAGE_DVA_CLAIM',         1 << 18, 'zio_dva_claim'        )   # ---C-
        self.stageBase('ZIO_STAGE_READY',             1 << 19, 'zio_ready'            )   # RWFCI
        self.stageBase('ZIO_STAGE_VDEV_IO_START',     1 << 20, 'zio_vdev_io_start'    )   # RW--I
        self.stageBase('ZIO_STAGE_VDEV_IO_DONE',      1 << 21, 'zio_vdev_io_done'     )   # RW--I
        self.stageBase('ZIO_STAGE_VDEV_IO_ASSESS',    1 << 22, 'zio_vdev_io_assess'   )   # RW--I
        self.stageBase('ZIO_STAGE_CHECKSUM_VERIFY',   1 << 23, 'zio_checksum_verify'  )   # R----
        self.stageBase('ZIO_STAGE_DONE',              1 << 24, 'zio_done'             )   # RWFCI
        
        self.stageBias('ZIO_INTERLOCK_STAGES', [
            'ZIO_STAGE_READY',
            'ZIO_STAGE_DONE'
        ])

        self.stageBias('ZIO_INTERLOCK_PIPELINE', [
            'ZIO_INTERLOCK_STAGES'
        ])

        self.stageBias('ZIO_VDEV_IO_STAGES', [
            'ZIO_STAGE_VDEV_IO_START',
            'ZIO_STAGE_VDEV_IO_DONE',
            'ZIO_STAGE_VDEV_IO_ASSESS'
        ])

        self.stageBias('ZIO_VDEV_CHILD_PIPELINE', [
            'ZIO_VDEV_IO_STAGES',
            'ZIO_STAGE_DONE'
        ])

        self.stageBias('ZIO_READ_COMMON_STAGES', [
            'ZIO_INTERLOCK_STAGES',
            'ZIO_VDEV_IO_STAGES',
            'ZIO_STAGE_CHECKSUM_VERIFY'
        ])

        self.stageBias('ZIO_READ_PHYS_PIPELINE', [
            'ZIO_READ_COMMON_STAGES'
        ])

        self.stageBias('ZIO_READ_PIPELINE', [
            'ZIO_READ_COMMON_STAGES',
            'ZIO_STAGE_READ_BP_INIT'
        ])

        self.stageBias('ZIO_DDT_CHILD_READ_PIPELINE', [
            'ZIO_READ_COMMON_STAGES'
        ])

        self.stageBias('ZIO_DDT_READ_PIPELINE', [
            'ZIO_INTERLOCK_STAGES',
            'ZIO_STAGE_READ_BP_INIT',
            'ZIO_STAGE_DDT_READ_START',
            'ZIO_STAGE_DDT_READ_DONE'
        ])

        self.stageBias('ZIO_WRITE_COMMON_STAGES', [
            'ZIO_INTERLOCK_STAGES',
            'ZIO_VDEV_IO_STAGES',
            'ZIO_STAGE_ISSUE_ASYNC',
            'ZIO_STAGE_CHECKSUM_GENERATE'
        ])

        self.stageBias('ZIO_WRITE_PHYS_PIPELINE', [
            'ZIO_WRITE_COMMON_STAGES'
        ])

        self.stageBias('ZIO_REWRITE_PIPELINE', [
            'ZIO_WRITE_COMMON_STAGES',
            'ZIO_STAGE_WRITE_COMPRESS',
            'ZIO_STAGE_ENCRYPT',
            'ZIO_STAGE_WRITE_BP_INIT'
        ])

        self.stageBias('ZIO_WRITE_PIPELINE', [
            'ZIO_WRITE_COMMON_STAGES',
            'ZIO_STAGE_WRITE_BP_INIT',
            'ZIO_STAGE_WRITE_COMPRESS',
            'ZIO_STAGE_ENCRYPT',
            'ZIO_STAGE_DVA_THROTTLE',
            'ZIO_STAGE_DVA_ALLOCATE'
        ])

        self.stageBias('ZIO_DDT_CHILD_WRITE_PIPELINE', [
            'ZIO_INTERLOCK_STAGES',
            'ZIO_VDEV_IO_STAGES',
            'ZIO_STAGE_DVA_THROTTLE',
            'ZIO_STAGE_DVA_ALLOCATE'
        ])

        self.stageBias('ZIO_DDT_WRITE_PIPELINE', [
            'ZIO_INTERLOCK_STAGES',
            'ZIO_STAGE_WRITE_BP_INIT',
            'ZIO_STAGE_ISSUE_ASYNC',
            'ZIO_STAGE_WRITE_COMPRESS',
            'ZIO_STAGE_ENCRYPT',
            'ZIO_STAGE_CHECKSUM_GENERATE',
            'ZIO_STAGE_DDT_WRITE'
        ])

        self.stageBias('ZIO_GANG_STAGES', [
            'ZIO_STAGE_GANG_ASSEMBLE',
            'ZIO_STAGE_GANG_ISSUE'
        ])

        self.stageBias('ZIO_FREE_PIPELINE', [
            'ZIO_INTERLOCK_STAGES',
            'ZIO_STAGE_FREE_BP_INIT',
            'ZIO_STAGE_DVA_FREE'
        ])

        self.stageBias('ZIO_DDT_FREE_PIPELINE', [
            'ZIO_INTERLOCK_STAGES',
            'ZIO_STAGE_FREE_BP_INIT',
            'ZIO_STAGE_ISSUE_ASYNC',
            'ZIO_STAGE_DDT_FREE'
        ])

        self.stageBias('ZIO_CLAIM_PIPELINE', [
            'ZIO_INTERLOCK_STAGES',
            'ZIO_STAGE_DVA_CLAIM'
        ])

        self.stageBias('ZIO_IOCTL_PIPELINE', [
            'ZIO_INTERLOCK_STAGES',
            'ZIO_STAGE_VDEV_IO_START',
            'ZIO_STAGE_VDEV_IO_ASSESS'
        ])

        self.stageBias('ZIO_TRIM_PIPELINE', [
            'ZIO_INTERLOCK_STAGES',
            'ZIO_STAGE_ISSUE_ASYNC',
            'ZIO_VDEV_IO_STAGES'
        ])

        self.stageBias('ZIO_BLOCKING_STAGES', [
            'ZIO_STAGE_DVA_ALLOCATE',
            'ZIO_STAGE_DVA_CLAIM',
            'ZIO_STAGE_VDEV_IO_START'
        ])

if __name__ == '__main__':
    if len(sys.argv) == 1:
        print('Usage: %s <ZIO_STAGE_...[|ZIO_STAGE_...]> ...' %
            os.path.basename(sys.argv[0]))
        sys.exit(0)
    
    zioStage = ZioStage()
    for arg in sys.argv[1:]:
        print(arg + '=>')
        print('\t' + '\n\t'.join(zioStage.detailStage(arg)))
