/* Generated by ./xlat/gen.sh from ./xlat/bsg_subprotocol.in; do not edit. */

#include "gcc_compat.h"
#include "static_assert.h"


#ifndef XLAT_MACROS_ONLY

# ifdef IN_MPERS

#  error static const struct xlat bsg_subprotocol in mpers mode

# else

static
const struct xlat bsg_subprotocol[] = {
#if defined(BSG_SUB_PROTOCOL_SCSI_CMD) || (defined(HAVE_DECL_BSG_SUB_PROTOCOL_SCSI_CMD) && HAVE_DECL_BSG_SUB_PROTOCOL_SCSI_CMD)
  XLAT(BSG_SUB_PROTOCOL_SCSI_CMD),
#endif
#if defined(BSG_SUB_PROTOCOL_SCSI_TMF) || (defined(HAVE_DECL_BSG_SUB_PROTOCOL_SCSI_TMF) && HAVE_DECL_BSG_SUB_PROTOCOL_SCSI_TMF)
  XLAT(BSG_SUB_PROTOCOL_SCSI_TMF),
#endif
#if defined(BSG_SUB_PROTOCOL_SCSI_TRANSPORT) || (defined(HAVE_DECL_BSG_SUB_PROTOCOL_SCSI_TRANSPORT) && HAVE_DECL_BSG_SUB_PROTOCOL_SCSI_TRANSPORT)
  XLAT(BSG_SUB_PROTOCOL_SCSI_TRANSPORT),
#endif
 XLAT_END
};

# endif /* !IN_MPERS */

#endif /* !XLAT_MACROS_ONLY */
