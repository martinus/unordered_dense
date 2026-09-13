# Real world usage

[README](../README.md) · [Usage](usage.md) · [Design](design.md) · [Benchmarks](benchmarks.md) · **Real world usage**

Open source projects that use `ankerl::unordered_dense`, grouped by what they do. The list was first put together on 2023-09-10 and last refreshed on 2026-08-06; every entry was confirmed by finding the include or the namespace in the project's own source on its default branch. Some authors have written in, the rest come from searching GitHub. Please send me a note if you want to be on that list!

## Databases and data engines

* [AliSQL](https://github.com/alibaba/AliSQL) - A MySQL branch originated from Alibaba Group.
* [ArcticDB](https://github.com/man-group/ArcticDB) - A high performance, serverless DataFrame database built for the Python Data Science ecosystem.
* [Bodo](https://github.com/bodo-ai/Bodo) - A high performance compute engine for Python data processing.
* [Milvus](https://github.com/milvus-io/milvus) - A high-performance, cloud-native vector database built for scalable vector search.
* [MySQL](https://github.com/mysql/mysql-server) - Binary log transaction dependency tracking has used this map since 8.4.3 and 9.1.0, replacing a tree for the writeset history and taking about 60% less space for it.
* [Percona Server](https://github.com/percona/percona-server) - A free, fully compatible, enhanced and open source drop-in replacement for MySQL.
* [Percona XtraBackup](https://github.com/percona/percona-xtrabackup) - Open source hot backup tool for InnoDB and XtraDB databases.
* [RonDB](https://github.com/logicalclocks/rondb) - A distribution of NDB Cluster for real-time applications with high availability.

## Games, emulators and game engines

* [Citron](https://github.com/citron-neo/emulator) - A Nintendo Switch emulator.
* [CrystalEngine](https://github.com/neilmewada/CrystalEngine) - A Vulkan game engine with FrameGraph, PBR rendering and a declarative UI framework.
* [DevilutionX](https://github.com/diasurgical/DevilutionX) - Diablo build for modern operating systems.
* [FEX](https://github.com/FEX-Emu/FEX) - A fast usermode x86 and x86-64 emulator for Arm64 Linux.
* [FOnline Engine](https://github.com/cvet/fonline) - A flexible cross-platform isometric game engine for multiplayer games.
* [HiveWE](https://github.com/stijnherfst/HiveWE) - A Warcraft III World Editor (WE) that focusses on speed and ease of use.
* [impacto](https://github.com/CommitteeOfZero/impacto) - A reimplementation of the "MAGES." visual novel engine.
* [LandSandBoat](https://github.com/LandSandBoat/server) - A server emulator for Final Fantasy XI.
* [Marathon Recompiled](https://github.com/sonicnext-dev/MarathonRecomp) - An unofficial PC port of the Xbox 360 version of Sonic the Hedgehog (2006), created via static recompilation.
* [Nazara Engine](https://github.com/NazaraEngine/NazaraEngine) - A cross-platform framework aimed at (but not limited to) real-time applications and games.
* [NVGT](https://github.com/samtupy/nvgt) - The Nonvisual Gaming Toolkit, a cross-platform audio game engine.
* [Oxylus Engine](https://github.com/oxylusengine/Oxylus) - A data-driven Vulkan game engine built in C++.
* [Project Alice](https://github.com/schombert/Project-Alice) - An open source recreation of the grand strategy game Victoria II.
* [Unleashed Recompiled](https://github.com/hedge-dev/UnleashedRecomp) - An unofficial PC port of the Xbox 360 version of Sonic Unleashed, created via static recompilation.
* [Visual Pinball](https://github.com/vpinball/vpinball) - An open source pinball table editor and simulator.

## Graphics, rendering and GPU compute

* [AdaptiveCpp](https://github.com/AdaptiveCpp/AdaptiveCpp) - Compiler for multiple programming models (SYCL, C++ standard parallelism) for CPUs and GPUs from all vendors.
* [CyberFSR2](https://github.com/PotatoOfDoom/CyberFSR2) - Drop-in DLSS replacement with FSR 2.0 for various games such as Cyberpunk 2077.
* [D3D12_Research](https://github.com/simco50/D3D12_Research) - A hobby project to experiment with various modern rendering techniques in DirectX 12.
* [LuisaCompute](https://github.com/LuisaGroup/LuisaCompute) - High-performance rendering framework on stream architectures.
* [NVIDIA MDL SDK](https://github.com/NVIDIA/MDL-SDK) - The NVIDIA Material Definition Language SDK, for physically based material definitions in rendering applications.
* [OptiScaler](https://github.com/optiscaler/OptiScaler) - Bridges upscaling and frame generation across GPUs, supporting DLSS2+, XeSS and FSR2+ inputs.
* [Skyrim Community Shaders](https://github.com/community-shaders/skyrim-community-shaders) - Community-driven advanced graphics modifications for Skyrim AE, SE and VR.
* [Slang](https://github.com/shader-slang/slang) - A shading language that makes it easier to build and maintain large shader codebases in a modular and extensible fashion.
* [WinUI](https://github.com/microsoft/microsoft-ui-xaml) - A modern UI framework with a rich set of controls and styles, the native UI layer of the Windows App SDK.

## Maps and geospatial

* [Cloudini](https://github.com/facontidavide/cloudini) - A point cloud compression library, with ROS/PCL integration.
* [CoMaps](https://codeberg.org/comaps/comaps) - Privacy-focused offline maps and navigation for Android and iOS, based on OpenStreetMap data.
* [HDMapping](https://github.com/MapsHD/HDMapping) - Open source software for mobile mapping, LiDAR odometry and point cloud registration.
* [MapLibre Native](https://github.com/maplibre/maplibre-native) - Interactive vector tile maps for iOS, Android and other platforms.
* [Valhalla](https://github.com/valhalla/valhalla) - Open source routing engine for OpenStreetMap data. Replaced robin-hood-hashing with this map and set in 3.6.0.

## CAD, 3D printing and simulation

* [Bambu Studio](https://github.com/bambulab/BambuStudio) - PC software for BambuLab and other 3D printers.
* [Lethe](https://github.com/chaos-polymtl/lethe) - Open-source computational fluid dynamics (CFD) software which uses high-order continuous Galerkin formulations to solve the incompressible Navier–Stokes equations (among others).
* [PrusaSlicer](https://github.com/prusa3d/PrusaSlicer) - G-code generator for 3D printers (RepRap, Makerbot, Ultimaker etc.).
* [web-ifc](https://github.com/ThatOpen/engine_web-ifc) - Reading and writing IFC files with Javascript, at native speeds.

## Bioinformatics

* [GW](https://github.com/kcleal/gw) - Genome browser and variant annotation tool for interactive visualisation of sequencing data.
* [kallisto](https://github.com/pachterlab/kallisto) - Near-optimal RNA-Seq quantification.
* [MashMap](https://github.com/marbl/MashMap) - A fast approximate aligner for long DNA sequences.
* [metaMDBG](https://github.com/GaetanBenoitDev/metaMDBG) - A lightweight assembler for long and accurate metagenomics reads.
* [wfmash](https://github.com/waveygang/wfmash) - Base-accurate DNA sequence alignments using WFA and mashmap3.

## Networking, media and security

* [Kismet](https://github.com/kismetwireless/kismet) - A sniffer, WIDS and wardriving tool for Wi-Fi, Bluetooth, Zigbee and RF, which runs on Linux and macOS.
* [libossia](https://github.com/ossia/libossia) - A modern C++, cross-environment distributed object model for creative coding and interaction scoring.
* [mediasoup](https://github.com/versatica/mediasoup) - Cutting edge WebRTC video conferencing SFU.
* [ossia score](https://github.com/ossia/score) - A free, open-source, cross-platform intermedia sequencer for precise and flexible scripting of interactive scenarios.
* [Rspamd](https://github.com/rspamd/rspamd) - Fast, free and open-source spam filtering system.
* [YANET](https://github.com/yanet-platform/yanet) - A high performance framework for forwarding traffic based on DPDK.

## Finance and blockchain

* [Cartesi Machine Emulator](https://github.com/cartesi/machine-emulator) - The off-chain RISC-V emulator implementation of the Cartesi Machine.
* [Monad](https://github.com/category-labs/monad) - A high-performance EVM-compatible layer-1 blockchain client.
* [opentxs](https://github.com/Open-Transactions/opentxs) - A free-software toolkit implementing the OTX protocol, together with a financial cryptography library, API, GUI, command-line interface and prototype notary server.
* [RISC Zero](https://github.com/risc0/risc0) - A zero-knowledge verifiable general computing platform based on RISC-V.
* [WonderTrader](https://github.com/wondertrader/wondertrader) - A one-stop quantitative research and trading framework.

## Tools, libraries and machine learning

* [ArkScript](https://github.com/ArkScript-lang/Ark) - A small, fast, functional and scripting language for C++ projects.
* [File Commander](https://github.com/VioletGiraffe/file-commander) - A cross-platform Total Commander-like orthodox file manager for Windows, Mac and Linux.
* [FlashTokenizer](https://github.com/NLPOptimize/flash-tokenizer) - An efficient and optimized BERT tokenizer engine for LLM inference serving.
* [Ichor](https://github.com/volt-software/Ichor) - A C++20 microservice bootstrapping framework focused on thread safety and dependency injection.
* [minigpt4.cpp](https://github.com/Maknee/minigpt4.cpp) - Port of MiniGPT4 in C++ (4bit, 5bit, 6bit, 8bit, 16bit CPU inference with GGML).
* [Nimble Commander](https://github.com/mikekazakov/nimble-commander) - A dual-pane file manager for macOS.
* [Operon](https://github.com/heal-research/operon) - A modern C++ framework for symbolic regression that uses genetic programming to find the best-fitting model for a given regression target.
* [PECOS](https://github.com/amzn/pecos) - A versatile and modular machine learning framework for fast learning and inference on problems with large output spaces, such as extreme multi-label ranking and large-scale retrieval.
* [PlotJuggler](https://github.com/PlotJuggler/PlotJuggler) - The time series visualization tool that you deserve.
* [PyOptInterface](https://github.com/metab0t/PyOptInterface) - Efficient modeling interface for mathematical optimization in Python.
* [STP](https://github.com/stp/stp) - Simple Theorem Prover, an efficient SMT solver for bitvectors.
* [Tulip](https://github.com/Tulip-Dev/tulip) - Large graphs analysis, drawing and visualization framework.

## Ports

Reimplementations of this design in other languages. They are not maintained here, and are listed because people have found them useful.

* [HashMapC99](https://github.com/benanil/HashMapC99) - A cache-efficient, densely stored hash map in C99, by Anılcan Gülkaya. Useful where a C++17 header is not an option, such as embedded targets, and for shorter compile times and smaller binaries.
