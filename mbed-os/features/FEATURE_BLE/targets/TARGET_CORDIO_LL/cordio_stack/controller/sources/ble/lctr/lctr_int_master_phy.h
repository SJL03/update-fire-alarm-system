/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  Internal link layer controller PHY features (master) interface file.
 *
 *  Copyright (c) 2009-2018 ARM Ltd., all rights reserved.
 *  SPDX-License-Identifier: LicenseRef-PBL
 *
 *  This file and the related binary are licensed under the
 *  Permissive Binary License, Version 1.0 (the "License");
 *  you may not use these files except in compliance with the License.
 *
 *  You may obtain a copy of the License here:
 *  LICENSE-permissive-binary-license-1.0.txt and at
 *  https://www.mbed.com/licenses/PBL-1.0
 *
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */
/*************************************************************************************************/

#ifndef LCTR_INT_PHY_MASTER_H
#define LCTR_INT_PHY_MASTER_H

#include "lctr_int.h"
#include "lctr_int_conn.h"

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

/* State machine */
bool_t lctrMstLlcpExecutePhyUpdateSm(lctrConnCtx_t *pCtx, uint8_t event);

#ifdef __cplusplus
};
#endif

#endif /* LCTR_INT_PHY_MASTER_H */
