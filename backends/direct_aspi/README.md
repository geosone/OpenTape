# Direct ASPI backend

This fallback will discover a DOS ASPI manager through `SCSIMGR$`, enumerate
sequential-access devices, and submit SCSI commands directly.  It is intended
for drives that have no ASPITAPE-compatible character driver.
