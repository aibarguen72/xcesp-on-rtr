/**
 * @file    OnRtrRegister.cpp
 * @brief   XCESP-ON-RTR — ProcObject type registrations
 * @project xcesp-on-rtr
 * @date    2026-03-18
 *
 * Registers all RTR object types into ProcObjectRegistry via static
 * initializers so xcespproc can instantiate them by type name.
 *
 * Each REGISTER_PROC_OBJECT macro is also placed in the corresponding
 * .cpp file — this file is kept as the authoritative registration list.
 */

#include "ProcObjectRegistry.h"
#include "objects/RtrRouterPObj.h"
#include "objects/RtrVrfPObj.h"
#include "objects/RtrInterfacePObj.h"
#include "objects/RtrVlanPObj.h"
#include "objects/RtrStaticRoutePObj.h"

// Registrations are performed by the REGISTER_PROC_OBJECT macros in each
// object's .cpp file.  This translation unit exists to document the full
// type list and to force the linker to include all object modules when the
// library is linked with --whole-archive or via xcespproc's exsrc mechanism.
