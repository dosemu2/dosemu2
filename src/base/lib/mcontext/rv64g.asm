/*
 * Copyright 2007 Sandia Corporation. Under the terms of Contract
 * DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government retains
 * certain rights in this software.
 *
 * Copyright (c) 2007, Sandia Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Sandia Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY SANDIA CORPORATION ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL SANDIA CORPORATION BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Modifications for 64-bit RISC-V context switching 
 * integrated under the terms of the GNU General Public License v2 (GPLv2).
 */

.globl GET
.global GET
.type GET, %function
GET:
        sd    s1, 8(a0)
        sd    s2, 16(a0)
        sd    s3, 24(a0)
        sd    s4, 32(a0)
        sd    s5, 40(a0)
        sd    s6, 48(a0)
        sd    s7, 56(a0)
        sd    s8, 64(a0)
        sd    s9, 72(a0)
        sd    s10, 80(a0)
        sd    s11, 88(a0)
        sd    s0, 96(a0)
        sd    ra, 104(a0)
        sd    sp, 112(a0)
        addi  t0, a0, 120
        fsd   f8, 0(t0)
        fsd   f9, 8(t0)
        fsd   f18, 16(t0)
        fsd   f19, 24(t0)
        fsd   f20, 32(t0)
        fsd   f21, 40(t0)
        fsd   f22, 48(t0)
        fsd   f23, 56(t0)
        fsd   f24, 64(t0)
        fsd   f25, 72(t0)
        fsd   f26, 80(t0)
        fsd   f27, 88(t0)
        li    t1, 1
        sd    t1, 0(a0)
        li    a0, 0
        ret

.globl SET
.global SET
.type SET, %function
SET:
        ld    s1, 8(a0)
        ld    s2, 16(a0)
        ld    s3, 24(a0)
        ld    s4, 32(a0)
        ld    s5, 40(a0)
        ld    s6, 48(a0)
        ld    s7, 56(a0)
        ld    s8, 64(a0)
        ld    s9, 72(a0)
        ld    s10, 80(a0)
        ld    s11, 88(a0)
        ld    s0, 96(a0)
        ld    ra, 104(a0)
        ld    sp, 112(a0)
        addi  t0, a0, 120
        fld   f8, 0(t0)
        fld   f9, 8(t0)
        fld   f18, 16(t0)
        fld   f19, 24(t0)
        fld   f20, 32(t0)
        fld   f21, 40(t0)
        fld   f22, 48(t0)
        fld   f23, 56(t0)
        fld   f24, 64(t0)
        fld   f25, 72(t0)
        fld   f26, 80(t0)
        fld   f27, 88(t0)
        ld    a0, 0(a0)
        ret
