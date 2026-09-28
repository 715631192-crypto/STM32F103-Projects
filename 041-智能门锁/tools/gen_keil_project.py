#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
生成 Keil MDK 工程文件 SmartLock.uvprojx。

为什么要用脚本生成而不是手写
--------------------------------
Keil 的 .uvprojx 是逐文件登记的 XML，手写很容易漏文件或写错相对路径。
这里改成"扫描目录 + 明确排除"，每次增删源文件后重跑一次即可：

    python tools/gen_keil_project.py

工具链与参考工程保持一致（必须一致，否则移植层对不上）：
    ARMCC V5.06 update 7 (build 960)，即 AC5 / RVDS
    FreeRTOS 用 firmware/FreeRTOS/portable/RVDS/ARM_CM3（AC5 专用）
"""

from pathlib import Path

REPO = Path(__file__).resolve().parent.parent

# ---- Keil 安装位置（按本机实际情况填；参考工程也是这么写死的） ----
KEIL_PACKS = r"D:\Keil_v5\Packs"
CMSIS_INC = KEIL_PACKS + r"\ARM\CMSIS\5.8.0\CMSIS\Core\Include"
DFP_ROOT = KEIL_PACKS + r"\Keil\STM32F1xx_DFP\2.3.0"

FT_C, FT_ASM, FT_H = "1", "2", "5"


def rel(p: Path) -> str:
    return str(p.relative_to(REPO)).replace("/", "\\")


def glob_files(subdir: str, pattern: str, ftype: str, exclude=()):
    """返回 [(相对路径, FileType)]，按文件名排序，剔除 exclude 里的文件名。"""
    d = REPO / subdir
    out = []
    for p in sorted(d.glob(pattern)):
        if p.name in exclude:
            continue
        out.append((rel(p), ftype))
    return out


def parts(*chunks):
    out = []
    for c in chunks:
        out.extend(c)
    return out


# ---------------------------------------------------------------------------
# 分组定义
# ---------------------------------------------------------------------------
GROUPS = [
    # 启动文件（向量表 + 调用 SystemInit）
    ("Startup", [("startup_stm32f103xb.s", FT_ASM)]),

    # 本工程可移植安全核心（可在主机上跑单测的那部分）
    ("APP_Core", parts(
        glob_files("firmware/app/src", "*.c", FT_C),
        glob_files("firmware/app/include", "*.h", FT_H),
    )),

    # 板级适配层与应用任务层
    ("PLATFORM", parts(
        glob_files("firmware/stm32/src", "*.c", FT_C),
        glob_files("firmware/stm32/include", "*.h", FT_H),
    )),

    # FreeRTOS：内核取自参考工程，移植层用 RVDS(AC5) 那份
    ("FreeRTOS", parts(
        glob_files("firmware/FreeRTOS", "*.c", FT_C),
        [("firmware\\FreeRTOS\\portable\\RVDS\\ARM_CM3\\port.c", FT_C),
         ("firmware\\FreeRTOS\\portable\\MemMang\\heap_4.c", FT_C)],
        glob_files("firmware/FreeRTOS/include", "*.h", FT_H),
        [("firmware\\FreeRTOS\\portable\\RVDS\\ARM_CM3\\portmacro.h", FT_H)],
    )),

    # 参考工程的应用层：只编 app_wifi.c（OneNET 物模型，原样复用），
    # 其余 app_*.c 是参考工程自己的业务逻辑，与本工程重复，不参与编译。
    ("USER", parts(
        [("firmware\\USER\\app_wifi.c", FT_C)],
        glob_files("firmware/USER", "*.h", FT_H),
    )),

    # 参考工程驱动
    ("SYSTEM", parts(
        glob_files("firmware/SYSTEM", "*.c", FT_C),
        glob_files("firmware/SYSTEM", "*.h", FT_H),
    )),
    ("HARDWARE", parts(
        # key_4x4.c 用旧引脚表，键盘改由 board_port.c 自己扫描；
        # sensor.c 是门磁/防撬，本轮未启用（见 smart_lock_board_config.h）。
        glob_files("firmware/HARDWARE", "*.c", FT_C, exclude=("key_4x4.c", "sensor.c")),
        glob_files("firmware/HARDWARE", "*.h", FT_H),
    )),

    # 只取 cJSON + MqttKit；sha1/totp/base64/onenet_token 用本工程自己的
    # （同名头文件在包含路径里排在前面，且那 4 个 .c 不参与编译）
    ("MIDDLEWARE", [
        ("firmware\\MIDDLEWARE\\cJSON.c", FT_C),
        ("firmware\\MIDDLEWARE\\cJSON.h", FT_H),
        ("firmware\\MIDDLEWARE\\MqttKit.c", FT_C),
        ("firmware\\MIDDLEWARE\\MqttKit.h", FT_H),
    ]),

    # 标准外设库（直接用 Keil Pack 里的文件，与参考工程一致）
    ("StdPeriph", [
        (DFP_ROOT + r"\Device\Source\system_stm32f10x.c", FT_C),
        (DFP_ROOT + r"\Device\StdPeriph_Driver\src\misc.c", FT_C),
        (DFP_ROOT + r"\Device\StdPeriph_Driver\src\stm32f10x_gpio.c", FT_C),
        (DFP_ROOT + r"\Device\StdPeriph_Driver\src\stm32f10x_rcc.c", FT_C),
        (DFP_ROOT + r"\Device\StdPeriph_Driver\src\stm32f10x_usart.c", FT_C),
        (DFP_ROOT + r"\Device\StdPeriph_Driver\src\stm32f10x_spi.c", FT_C),
        (DFP_ROOT + r"\Device\StdPeriph_Driver\src\stm32f10x_i2c.c", FT_C),
        (DFP_ROOT + r"\Device\StdPeriph_Driver\src\stm32f10x_tim.c", FT_C),
        (DFP_ROOT + r"\Device\StdPeriph_Driver\src\stm32f10x_exti.c", FT_C),
        (DFP_ROOT + r"\Device\StdPeriph_Driver\src\stm32f10x_flash.c", FT_C),
        (DFP_ROOT + r"\Device\StdPeriph_Driver\src\stm32f10x_pwr.c", FT_C),
        (DFP_ROOT + r"\Device\StdPeriph_Driver\src\stm32f10x_bkp.c", FT_C),
        # 本工程 watchdog_init() 用到 IWDG（SMART_LOCK_ENABLE_IWDG=1 时）
        (DFP_ROOT + r"\Device\StdPeriph_Driver\src\stm32f10x_iwdg.c", FT_C),
    ]),
]

# 包含路径顺序很关键：本工程的同名头文件必须排在参考工程前面
INCLUDE_PATH = ";".join([
    r".\firmware\stm32\include",      # FreeRTOSConfig.h / board_port.h / pinmap
    r".\firmware\app\include",        # sha1.h / totp.h / base64.h / onenet_token.h
    r".\firmware\FreeRTOS\include",
    r".\firmware\FreeRTOS\portable\RVDS\ARM_CM3",
    r".\firmware\USER",               # app_*.h / stm32f10x_conf.h
    r".\firmware\SYSTEM",
    r".\firmware\HARDWARE",
    r".\firmware\MIDDLEWARE",
    CMSIS_INC,
    DFP_ROOT + r"\Device\Include",
    DFP_ROOT + r"\Device\StdPeriph_Driver\inc",
])

TEMPLATE = """<?xml version="1.0" encoding="UTF-8" standalone="no" ?>
<Project xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:noNamespaceSchemaLocation="project_projx.xsd">

  <SchemaVersion>2.1</SchemaVersion>

  <Header>### uVision Project, (C) Keil Software</Header>

  <Targets>
    <Target>
      <TargetName>STM32F103C8</TargetName>
      <ToolsetNumber>0x4</ToolsetNumber>
      <ToolsetName>ARM-ADS</ToolsetName>
      <pCCUsed>5060960::V5.06 update 7 (build 960)::.\\ARMCC</pCCUsed>
      <uAC6>0</uAC6>
      <TargetOption>
        <TargetCommonOption>
          <Device>STM32F103C8</Device>
          <Vendor>STMicroelectronics</Vendor>
          <PackID>Keil.STM32F1xx_DFP.2.3.0</PackID>
          <PackURL>http://www.keil.com/pack/</PackURL>
          <Cpu>IROM(0x08000000,0x10000) IRAM(0x20000000,0x5000) CPUTYPE("Cortex-M3") CLOCK(8000000) ELITTLE</Cpu>
          <FlashUtilSpec></FlashUtilSpec>
          <StartupFile></StartupFile>
          <FlashDriverDll></FlashDriverDll>
          <DeviceId></DeviceId>
          <RegisterFile></RegisterFile>
          <MemoryEnv></MemoryEnv>
          <Cmp></Cmp>
          <Asm></Asm>
          <Linker></Linker>
          <OHString></OHString>
          <InfinionOptionDll></InfinionOptionDll>
          <SLE66CMisc></SLE66CMisc>
          <SLE66AMisc></SLE66AMisc>
          <SLE66LinkerMisc></SLE66LinkerMisc>
          <SFDFile>$$Device:STM32F103C8$SVD\\STM32F103xx.svd</SFDFile>
          <bCustSvd>0</bCustSvd>
          <UseEnv>0</UseEnv>
          <BinPath></BinPath>
          <IncludePath></IncludePath>
          <LibPath></LibPath>
          <RegisterFilePath></RegisterFilePath>
          <DBRegisterFilePath></DBRegisterFilePath>
          <TargetStatus>
            <Error>0</Error>
            <ExitCodeStop>0</ExitCodeStop>
            <ButtonStop>0</ButtonStop>
            <NotGenerated>0</NotGenerated>
            <InvalidFlash>1</InvalidFlash>
          </TargetStatus>
          <OutputDirectory>.\\obj\\</OutputDirectory>
          <OutputName>SmartLock</OutputName>
          <CreateExecutable>1</CreateExecutable>
          <CreateLib>0</CreateLib>
          <CreateHexFile>1</CreateHexFile>
          <DebugInformation>1</DebugInformation>
          <BrowseInformation>0</BrowseInformation>
          <ListingPath>.\\listings\\</ListingPath>
          <HexFormatSelection>1</HexFormatSelection>
          <Merge32K>0</Merge32K>
          <CreateBatchFile>0</CreateBatchFile>
          <BeforeCompile>
            <RunUserProg1>0</RunUserProg1>
            <RunUserProg2>0</RunUserProg2>
            <UserProg1Name></UserProg1Name>
            <UserProg2Name></UserProg2Name>
            <UserProg1Dos16Mode>0</UserProg1Dos16Mode>
            <UserProg2Dos16Mode>0</UserProg2Dos16Mode>
            <nStopU1X>0</nStopU1X>
            <nStopU2X>0</nStopU2X>
          </BeforeCompile>
          <BeforeMake>
            <RunUserProg1>0</RunUserProg1>
            <RunUserProg2>0</RunUserProg2>
            <UserProg1Name></UserProg1Name>
            <UserProg2Name></UserProg2Name>
            <UserProg1Dos16Mode>0</UserProg1Dos16Mode>
            <UserProg2Dos16Mode>0</UserProg2Dos16Mode>
            <nStopB1X>0</nStopB1X>
            <nStopB2X>0</nStopB2X>
          </BeforeMake>
          <AfterMake>
            <RunUserProg1>0</RunUserProg1>
            <RunUserProg2>0</RunUserProg2>
            <UserProg1Name></UserProg1Name>
            <UserProg2Name></UserProg2Name>
            <UserProg1Dos16Mode>0</UserProg1Dos16Mode>
            <UserProg2Dos16Mode>0</UserProg2Dos16Mode>
            <nStopA1X>0</nStopA1X>
            <nStopA2X>0</nStopA2X>
          </AfterMake>
          <SelectedForBatchBuild>0</SelectedForBatchBuild>
          <SVCSIdString></SVCSIdString>
        </TargetCommonOption>
        <CommonProperty>
          <UseCPPCompiler>0</UseCPPCompiler>
          <RVCTCodeConst>0</RVCTCodeConst>
          <RVCTZI>0</RVCTZI>
          <RVCTOtherData>0</RVCTOtherData>
          <ModuleSelection>0</ModuleSelection>
          <IncludeInBuild>0</IncludeInBuild>
          <AlwaysBuild>0</AlwaysBuild>
          <GenerateAssemblyFile>0</GenerateAssemblyFile>
          <AssembleAssemblyFile>0</AssembleAssemblyFile>
          <PublicsOnly>0</PublicsOnly>
          <StopOnExitCode>3</StopOnExitCode>
          <CustomArgument></CustomArgument>
          <IncludeLibraryModules></IncludeLibraryModules>
          <ComprImg>1</ComprImg>
        </CommonProperty>
        <DllOption>
          <SimDllName></SimDllName>
          <SimDllArguments></SimDllArguments>
          <SimDlgDll>DLG.DLL</SimDlgDll>
          <SimDlgDllArguments></SimDlgDllArguments>
          <TargetDllName>SARMCM3.DLL</TargetDllName>
          <TargetDllArguments> "-MPU" </TargetDllArguments>
          <TargetDlgDll>TCM.DLL</TargetDlgDll>
          <TargetDlgDllArguments></TargetDlgDllArguments>
        </DllOption>
        <DebugOption>
          <OPTHX>
            <HexSelection>1</HexSelection>
            <HexRangeLowAddress>0</HexRangeLowAddress>
            <HexRangeHighAddress>0</HexRangeHighAddress>
            <HexOffset>0</HexOffset>
            <Oh166RecLen>16</Oh166RecLen>
          </OPTHX>
        </DebugOption>
        <Utilities>
          <Flash1>
            <UseTargetDll>1</UseTargetDll>
            <UseExternalTool>0</UseExternalTool>
            <RunIndependent>0</RunIndependent>
            <UpdateFlashBeforeDebugging>1</UpdateFlashBeforeDebugging>
            <Capability>1</Capability>
            <DriverSelection>-1</DriverSelection>
          </Flash1>
          <bUseTDR>1</bUseTDR>
          <Flash2>BIN\\UL2CM3.DLL</Flash2>
          <Flash3></Flash3>
          <Flash4></Flash4>
          <pFcarmOut></pFcarmOut>
          <pFcarmGrp></pFcarmGrp>
          <pFcArmRoot></pFcArmRoot>
          <FcArmLst>0</FcArmLst>
        </Utilities>
        <TargetArmAds>
          <ArmAdsMisc>
            <GenerateListings>1</GenerateListings>
            <asHll>1</asHll>
            <asAsm>1</asAsm>
            <asMacX>1</asMacX>
            <asSyms>1</asSyms>
            <asFals>1</asFals>
            <asDbgD>1</asDbgD>
            <asForm>1</asForm>
            <ldLst>0</ldLst>
            <ldmm>1</ldmm>
            <ldXref>1</ldXref>
            <BigEnd>0</BigEnd>
            <AdsALst>1</AdsALst>
            <AdsACrf>1</AdsACrf>
            <AdsANop>0</AdsANop>
            <AdsANot>0</AdsANot>
            <AdsLLst>1</AdsLLst>
            <AdsLmap>1</AdsLmap>
            <AdsLcgr>1</AdsLcgr>
            <AdsLsym>1</AdsLsym>
            <AdsLszi>1</AdsLszi>
            <AdsLtoi>1</AdsLtoi>
            <AdsLsun>1</AdsLsun>
            <AdsLven>1</AdsLven>
            <AdsLsxf>1</AdsLsxf>
            <RvctClst>0</RvctClst>
            <GenPPlst>0</GenPPlst>
            <AdsCpuType>"Cortex-M3"</AdsCpuType>
            <RvctDeviceName></RvctDeviceName>
            <mOS>0</mOS>
            <uocRom>0</uocRom>
            <uocRam>0</uocRam>
            <hadIROM>1</hadIROM>
            <hadIRAM>1</hadIRAM>
            <hadXRAM>0</hadXRAM>
            <uocXRam>0</uocXRam>
            <RvdsVP>0</RvdsVP>
            <RvdsMve>0</RvdsMve>
            <RvdsCdeCp>0</RvdsCdeCp>
            <hadIRAM2>0</hadIRAM2>
            <hadIROM2>0</hadIROM2>
            <StupSel>0</StupSel>
            <useUlib>1</useUlib>
            <EndSel>0</EndSel>
            <uLtcg>0</uLtcg>
            <nSecure>0</nSecure>
            <RoSelD>4</RoSelD>
            <RwSelD>9</RwSelD>
            <CodeSel>0</CodeSel>
            <OptFeed>0</OptFeed>
            <NoZi1>0</NoZi1>
            <NoZi2>0</NoZi2>
            <NoZi3>0</NoZi3>
            <NoZi4>0</NoZi4>
            <NoZi5>0</NoZi5>
            <Ro1Chk>0</Ro1Chk>
            <Ro2Chk>0</Ro2Chk>
            <Ro3Chk>0</Ro3Chk>
            <Ir1Chk>0</Ir1Chk>
            <Ir2Chk>0</Ir2Chk>
            <Ra1Chk>0</Ra1Chk>
            <Ra2Chk>0</Ra2Chk>
            <Ra3Chk>0</Ra3Chk>
            <Im1Chk>0</Im1Chk>
            <Im2Chk>0</Im2Chk>
            <OnChipMemories>
              <Ocm1>
                <Type>0</Type>
                <StartAddress>0x0</StartAddress>
                <Size>0x0</Size>
              </Ocm1>
              <Ocm2>
                <Type>0</Type>
                <StartAddress>0x0</StartAddress>
                <Size>0x0</Size>
              </Ocm2>
              <Ocm3>
                <Type>0</Type>
                <StartAddress>0x0</StartAddress>
                <Size>0x0</Size>
              </Ocm3>
              <Ocm4>
                <Type>0</Type>
                <StartAddress>0x0</StartAddress>
                <Size>0x0</Size>
              </Ocm4>
              <Ocm5>
                <Type>0</Type>
                <StartAddress>0x0</StartAddress>
                <Size>0x0</Size>
              </Ocm5>
              <Ocm6>
                <Type>0</Type>
                <StartAddress>0x0</StartAddress>
                <Size>0x0</Size>
              </Ocm6>
              <IRAM>
                <Type>0</Type>
                <StartAddress>0x20000000</StartAddress>
                <Size>0x5000</Size>
              </IRAM>
              <IROM>
                <Type>1</Type>
                <StartAddress>0x8000000</StartAddress>
                <Size>0x10000</Size>
              </IROM>
              <XRAM>
                <Type>0</Type>
                <StartAddress>0x0</StartAddress>
                <Size>0x0</Size>
              </XRAM>
              <OCR_RVCT1>
                <Type>1</Type>
                <StartAddress>0x0</StartAddress>
                <Size>0x0</Size>
              </OCR_RVCT1>
              <OCR_RVCT2>
                <Type>1</Type>
                <StartAddress>0x0</StartAddress>
                <Size>0x0</Size>
              </OCR_RVCT2>
              <OCR_RVCT3>
                <Type>1</Type>
                <StartAddress>0x0</StartAddress>
                <Size>0x0</Size>
              </OCR_RVCT3>
              <OCR_RVCT4>
                <Type>1</Type>
                <StartAddress>0x8000000</StartAddress>
                <Size>0x10000</Size>
              </OCR_RVCT4>
              <OCR_RVCT5>
                <Type>1</Type>
                <StartAddress>0x0</StartAddress>
                <Size>0x0</Size>
              </OCR_RVCT5>
              <OCR_RVCT6>
                <Type>0</Type>
                <StartAddress>0x0</StartAddress>
                <Size>0x0</Size>
              </OCR_RVCT6>
              <OCR_RVCT7>
                <Type>0</Type>
                <StartAddress>0x0</StartAddress>
                <Size>0x0</Size>
              </OCR_RVCT7>
              <OCR_RVCT8>
                <Type>0</Type>
                <StartAddress>0x0</StartAddress>
                <Size>0x0</Size>
              </OCR_RVCT8>
              <OCR_RVCT9>
                <Type>0</Type>
                <StartAddress>0x20000000</StartAddress>
                <Size>0x5000</Size>
              </OCR_RVCT9>
              <OCR_RVCT10>
                <Type>0</Type>
                <StartAddress>0x0</StartAddress>
                <Size>0x0</Size>
              </OCR_RVCT10>
            </OnChipMemories>
            <RvctStartVector></RvctStartVector>
          </ArmAdsMisc>
          <Cads>
            <interw>1</interw>
            <Optim>1</Optim>
            <oTime>0</oTime>
            <SplitLS>0</SplitLS>
            <OneElfS>1</OneElfS>
            <Strict>0</Strict>
            <EnumInt>0</EnumInt>
            <PlainCh>0</PlainCh>
            <Ropi>0</Ropi>
            <Rwpi>0</Rwpi>
            <wLevel>2</wLevel>
            <uThumb>0</uThumb>
            <uSurpInc>0</uSurpInc>
            <uC99>1</uC99>
            <uGnu>0</uGnu>
            <useXO>0</useXO>
            <v6Lang>3</v6Lang>
            <v6LangP>3</v6LangP>
            <vShortEn>1</vShortEn>
            <vShortWch>1</vShortWch>
            <v6Lto>0</v6Lto>
            <v6WtE>0</v6WtE>
            <v6Rtti>0</v6Rtti>
            <VariousControls>
              <MiscControls></MiscControls>
              <Define>USE_STDPERIPH_DRIVER,STM32F10X_MD</Define>
              <Undefine></Undefine>
              <IncludePath>__INCLUDE_PATH__</IncludePath>
            </VariousControls>
          </Cads>
          <Aads>
            <interw>1</interw>
            <Ropi>0</Ropi>
            <Rwpi>0</Rwpi>
            <thumb>0</thumb>
            <SplitLS>0</SplitLS>
            <SwStkChk>0</SwStkChk>
            <NoWarn>0</NoWarn>
            <uSurpInc>0</uSurpInc>
            <useXO>0</useXO>
            <ClangAsOpt>1</ClangAsOpt>
            <VariousControls>
              <MiscControls></MiscControls>
              <Define></Define>
              <Undefine></Undefine>
              <IncludePath></IncludePath>
            </VariousControls>
          </Aads>
          <LDads>
            <umfTarg>0</umfTarg>
            <Ropi>0</Ropi>
            <Rwpi>0</Rwpi>
            <noStLib>0</noStLib>
            <RepFail>1</RepFail>
            <useFile>1</useFile>
            <TextAddressRange></TextAddressRange>
            <DataAddressRange></DataAddressRange>
            <pXoBase></pXoBase>
            <ScatterFile>.\\STM32F103C8.sct</ScatterFile>
            <IncludeLibs></IncludeLibs>
            <IncludeLibsPath></IncludeLibsPath>
            <Misc></Misc>
            <LinkerInputFile></LinkerInputFile>
            <DisabledWarnings></DisabledWarnings>
          </LDads>
        </TargetArmAds>
      </TargetOption>
      <Groups>
__GROUPS__
      </Groups>
    </Target>
  </Targets>

  <RTE>
    <apis/>
    <components/>
    <files/>
  </RTE>

</Project>
"""


def build_groups_xml():
    lines = []
    for name, files in GROUPS:
        lines.append("        <Group>")
        lines.append("          <GroupName>%s</GroupName>" % name)
        lines.append("          <Files>")
        for path, ftype in files:
            fname = path.split("\\")[-1]
            lines.append("            <File>")
            lines.append("              <FileName>%s</FileName>" % fname)
            lines.append("              <FileType>%s</FileType>" % ftype)
            lines.append("              <FilePath>%s</FilePath>" % path)
            lines.append("            </File>")
        lines.append("          </Files>")
        lines.append("        </Group>")
    return "\n".join(lines)


def main():
    missing = []
    for name, files in GROUPS:
        for path, _ in files:
            if path.startswith(DFP_ROOT):
                continue
            if not (REPO / path.replace("\\", "/")).exists():
                missing.append("%s (%s)" % (path, name))
    if missing:
        raise SystemExit("以下文件不存在，工程未生成：\n  " + "\n  ".join(missing))

    xml = TEMPLATE.replace("__GROUPS__", build_groups_xml())
    xml = xml.replace("__INCLUDE_PATH__", INCLUDE_PATH)
    out = REPO / "SmartLock.uvprojx"
    out.write_text(xml, encoding="utf-8")

    total = sum(len(f) for _, f in GROUPS)
    print("生成 %s" % out)
    print("共 %d 个文件，%d 个分组" % (total, len(GROUPS)))
    for name, files in GROUPS:
        c = len([1 for _, t in files if t == FT_C])
        a = len([1 for _, t in files if t == FT_ASM])
        h = len([1 for _, t in files if t == FT_H])
        print("  %-12s C=%-3d asm=%-2d hdr=%-3d" % (name, c, a, h))


if __name__ == "__main__":
    main()
