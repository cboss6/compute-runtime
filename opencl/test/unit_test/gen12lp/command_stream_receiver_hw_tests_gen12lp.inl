/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/test/unit_test/cmd_parse/hw_parse.h"

#include "opencl/source/command_queue/command_queue_hw.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/fixtures/ult_command_stream_receiver_fixture.h"
#include "opencl/test/unit_test/gen12lp/special_ult_helper_gen12lp.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_csr.h"
#include "opencl/test/unit_test/mocks/mock_event.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "test.h"

#include "gtest/gtest.h"
#include "reg_configs_common.h"

using namespace NEO;

#include "opencl/test/unit_test/command_stream/command_stream_receiver_hw_tests.inl"

using CommandStreamReceiverHwTestGen12lp = CommandStreamReceiverHwTest<TGLLPFamily>;

GEN12LPTEST_F(CommandStreamReceiverHwTestGen12lp, givenPreambleSentWhenL3ConfigRequestChangedThenDontProgramL3Register) {
    size_t GWS = 1;
    MockContext ctx(pClDevice);
    MockKernelWithInternals kernel(*pClDevice);
    CommandQueueHw<FamilyType> commandQueue(&ctx, pClDevice, 0, false);
    auto commandStreamReceiver = new MockCsrHw<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex());
    pDevice->resetCommandStreamReceiver(commandStreamReceiver);
    auto &commandStreamCSR = commandStreamReceiver->getCS();

    commandStreamReceiver->isPreambleSent = true;
    commandStreamReceiver->lastSentL3Config = 0;

    commandQueue.enqueueKernel(kernel, 1, nullptr, &GWS, nullptr, 0, nullptr, nullptr);

    parseCommands<FamilyType>(commandStreamCSR, 0);
    auto itorCmd = findMmio<FamilyType>(cmdList.begin(), cmdList.end(), L3CNTLRegisterOffset<FamilyType>::registerOffset);
    ASSERT_EQ(cmdList.end(), itorCmd);
}

GEN12LPTEST_F(CommandStreamReceiverHwTestGen12lp, whenProgrammingMiSemaphoreWaitThenSetRegisterPollModeMemoryPoll) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    MI_SEMAPHORE_WAIT miSemaphoreWait = FamilyType::cmdInitMiSemaphoreWait;
    EXPECT_EQ(MI_SEMAPHORE_WAIT::REGISTER_POLL_MODE::REGISTER_POLL_MODE_MEMORY_POLL, miSemaphoreWait.getRegisterPollMode());
}

using CommandStreamReceiverFlushTaskTests = UltCommandStreamReceiverTest;
GEN12LPTEST_F(UltCommandStreamReceiverTest, givenStateBaseAddressWhenItIsRequiredThenThereIsPipeControlPriorToItWithTextureCacheFlushAndHdc) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    configureCSRtoNonDirtyState<FamilyType>(false);
    ioh.replaceBuffer(ptrOffset(ioh.getCpuBase(), +1u), ioh.getMaxAvailableSpace() + MemoryConstants::pageSize * 3);

    flushTask(commandStreamReceiver);

    parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    auto stateBaseAddressItor = find<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    auto pipeControlItor = find<typename FamilyType::PIPE_CONTROL *>(cmdList.begin(), stateBaseAddressItor);
    EXPECT_NE(stateBaseAddressItor, pipeControlItor);
    auto pipeControlCmd = reinterpret_cast<typename FamilyType::PIPE_CONTROL *>(*pipeControlItor);
    EXPECT_TRUE(pipeControlCmd->getTextureCacheInvalidationEnable());
    EXPECT_TRUE(pipeControlCmd->getDcFlushEnable());
    EXPECT_TRUE(pipeControlCmd->getHdcPipelineFlush());
}
