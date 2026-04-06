# Android GEMS

On-device agent-native multimodal generation with memory and skills, ported from [GEMS](https://github.com/lcqysl/GEMS) to run fully on Android.

## What It Does

GEMS uses an agent loop to iteratively improve text-to-image generation:

1. **Decompose** — breaks your prompt into verifiable requirements ("Is there a book?", "Is the lighting golden?")
2. **Generate** — creates an image with Stable Diffusion Turbo (Vulkan GPU)
3. **Verify** — checks each requirement against the generated image (Gemma 4)
4. **Refine** — rewrites the prompt to fix failures
5. **Repeat** — generates an improved image with the refined prompt

The app shows a side-by-side comparison of direct generation vs the GEMS agent loop.

### Example: "a mountain range at sunrise"

| Direct Generation | GEMS Output |
|---|---|
| ![Direct](docs/images/direct_mountain_range_at_sunrise.png) | ![GEMS](docs/images/gems_mountain_range_at_sunrise.png) |

The GEMS agent triggered the **landscape** skill, which enhanced the prompt with detailed instructions about atmospheric depth, natural lighting, and composition. The GEMS-enhanced output shows a mountain scene with a lake reflection, wildflowers, and dramatic sky — significantly more detailed than the direct generation.

### Example: "a girl waiting at a train station at sunset with anime style"

| Direct Generation | GEMS Output |
|---|---|
| ![Direct](docs/images/direct_shinkai_train_station.png) | ![GEMS](docs/images/gems_shinkai_train_station.png) |

The GEMS agent triggered the **anime (Makoto Shinkai)** skill, which enhanced the prompt with Shinkai's signature cinematic details — volumetric god rays, dramatic cumulonimbus clouds transitioning from orange to magenta, lens flare from the setting sun, hyper-detailed station architecture, and wet platform reflections. The GEMS-enhanced output captures the breathtaking photorealistic-meets-anime look of films like Your Name.

## Stack

| Component | Implementation |
|---|---|
| LLM | Gemma 4 E2B via LiteRT-LM (GPU, ~1-2s/call) |
| Image Gen | SD Turbo via stable-diffusion.cpp + Vulkan GPU (~15-30s) |
| Agent Loop | Kotlin port of GEMS.py (Decompose → Generate → Verify → Refine) |
| UI | Jetpack Compose + Material 3 |
| DI | Hilt |
| DB | Room (agent memory persistence) |

## Requirements

- **Hardware**: Android device with Vulkan GPU support (tested on Pixel 9 / Tensor G4 / Android 16)
- **Storage**: ~8GB free on device for model files
- **Host OS**: macOS (Intel or Apple Silicon) or Linux

---

## Setup Guide (from scratch)

### Step 1: Install Java (JDK 17+)

**macOS:**
```bash
brew install openjdk@17
```

**Linux (Ubuntu/Debian):**
```bash
sudo apt install openjdk-17-jdk
```

Verify:
```bash
java -version   # Should show 17+
```

### Step 2: Install Android Studio

1. Download from https://developer.android.com/studio
2. Install and open Android Studio
3. Complete the setup wizard — it will install:
   - Android SDK (API 35)
   - Android SDK Build-Tools
   - Android SDK Platform-Tools (includes `adb`)

After installation, note your SDK path:
- **macOS**: `~/Library/Android/sdk`
- **Linux**: `~/Android/Sdk`

### Step 3: Install Android NDK

Open Android Studio → Settings → Languages & Frameworks → Android SDK → SDK Tools tab:
- Check **NDK (Side by side)** → Install version **25.1.8937393** or later
- Check **CMake** → Install

Or via command line:
```bash
$ANDROID_HOME/cmdline-tools/latest/bin/sdkmanager "ndk;25.1.8937393" "cmake;3.22.1"
```

### Step 4: Set environment variables

Add to your `~/.zshrc` or `~/.bashrc`:
```bash
export ANDROID_HOME=~/Library/Android/sdk        # macOS
# export ANDROID_HOME=~/Android/Sdk              # Linux
export JAVA_HOME="/Applications/Android Studio.app/Contents/jbr/Contents/Home"  # macOS
# export JAVA_HOME=/usr/lib/jvm/java-17-openjdk  # Linux
export PATH=$ANDROID_HOME/platform-tools:$PATH
```

Reload:
```bash
source ~/.zshrc
```

Verify:
```bash
adb --version    # Should work
java -version    # Should show 17+
```

### Step 5: Install Python dependencies

Python 3.10+ is needed for downloading models:
```bash
pip3 install huggingface_hub
```

### Step 6: Install build tools

**macOS:**
```bash
brew install cmake git
```

**Linux:**
```bash
sudo apt install cmake git build-essential
```

---

## Build & Run

### Step 7: Clone the repo

```bash
git clone <repo-url>
cd android_gems
```

### Step 8: Download model files

The app needs three model files (~4.3GB total). You can download them **directly on your phone** (recommended) or via command line.

#### Option A: Download in-app (recommended)

After installing and launching the app (steps 11-13), tap **"Download Models"** on the home screen. The built-in model manager downloads all three models directly to the device:

| Model | Size | Source |
|---|---|---|
| SD Turbo (Image Generator) | 1.9GB | [Green-Sky/SD-Turbo-GGUF](https://huggingface.co/Green-Sky/SD-Turbo-GGUF) |
| TAESD (Fast Decoder) | 9MB | [madebyollin/taesd](https://huggingface.co/madebyollin/taesd) |
| Gemma 4 E2B (LLM) | 2.4GB | [litert-community/gemma-4-E2B-it-litert-lm](https://huggingface.co/litert-community/gemma-4-E2B-it-litert-lm) |

All models are publicly available — no authentication required. Downloads continue in the background if you navigate away.

Skip to **Step 9** if using this option.

#### Option B: Download via command line + adb push

```bash
cd models/
./download_models.sh    # downloads to models/ directory on your computer
```

Then connect your device and push:
```bash
./push_to_device.sh     # pushes all model files to the device
```

### Step 9: Connect your Android device

1. Enable **Developer Options** on your phone (Settings → About Phone → tap Build Number 7 times)
2. Enable **USB Debugging** (Settings → Developer Options → USB Debugging)
3. Connect via USB cable
4. Accept the debugging prompt on your phone

Verify:
```bash
adb devices
# Should show your device
```

### Step 10: Push models to device

```bash
cd models/
./push_to_device.sh
cd ..
```

This pushes all model files to `/data/local/tmp/` on the device.

### Step 11: Build the native image generation library

The image generator uses [stable-diffusion.cpp](https://github.com/leejet/stable-diffusion.cpp) with Vulkan GPU acceleration. Build it as a shared library for Android:

```bash
# Set NDK path
export NDK=$ANDROID_HOME/ndk/25.1.8937393

# Clone stable-diffusion.cpp (if not already in libs/)
git clone --recursive https://github.com/leejet/stable-diffusion.cpp.git libs/stable-diffusion.cpp

# Update Vulkan headers for C++ support
git clone --depth 1 https://github.com/KhronosGroup/Vulkan-Headers.git /tmp/Vulkan-Headers
cp /tmp/Vulkan-Headers/include/vulkan/*.hpp \
   $NDK/toolchains/llvm/prebuilt/*/sysroot/usr/include/vulkan/
cp /tmp/Vulkan-Headers/include/vulkan/*.h \
   $NDK/toolchains/llvm/prebuilt/*/sysroot/usr/include/vulkan/
mkdir -p $NDK/toolchains/llvm/prebuilt/*/sysroot/usr/include/vk_video
cp /tmp/Vulkan-Headers/include/vk_video/*.h \
   $NDK/toolchains/llvm/prebuilt/*/sysroot/usr/include/vk_video/

# Configure and build
cd libs/stable-diffusion.cpp
mkdir -p build-android && cd build-android

cmake .. \
  -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-33 \
  -DSD_VULKAN=ON -DGGML_VULKAN=ON \
  -DCMAKE_BUILD_TYPE=Release \
  -DVulkan_GLSLC_EXECUTABLE=$NDK/shader-tools/*/glslc

cmake --build . -j8 --target stable-diffusion

# Build JNI shared library
CLANG=$NDK/toolchains/llvm/prebuilt/*/bin/aarch64-linux-android33-clang++
OMP_STATIC=$NDK/toolchains/llvm/prebuilt/*/lib64/clang/*/lib/linux/aarch64/libomp.a

$CLANG -shared -fPIC -o libsdcpp.so ../jni_bridge.cpp -I.. \
  -Wl,--whole-archive \
  libstable-diffusion.a ggml/src/libggml.a ggml/src/libggml-base.a \
  ggml/src/libggml-cpu.a ggml/src/ggml-vulkan/libggml-vulkan.a \
  thirdparty/libwebp/libwebp.a thirdparty/libwebp/libsharpyuv.a \
  thirdparty/libwebp/libwebpmux.a $OMP_STATIC \
  -Wl,--no-whole-archive \
  -lvulkan -llog -landroid -lm -lz -ldl -static-libstdc++

# Strip and copy to app
$NDK/toolchains/llvm/prebuilt/*/bin/llvm-strip libsdcpp.so
mkdir -p ../../../app/src/main/jniLibs/arm64-v8a
cp libsdcpp.so ../../../app/src/main/jniLibs/arm64-v8a/

cd ../../..
```

### Step 12: Build and install the app

```bash
./gradlew assembleDebug
adb install -t app/build/outputs/apk/debug/app-debug.apk
```

### Step 13: Launch the app

```bash
adb shell am start -n com.gems.android/.ui.MainActivity
```

Or just tap the **Android GEMS** icon on your phone.

---

## Using the App

### Home Screen

- **Prompt field** — pre-filled with a default prompt, edit as you like
- **Image gen steps** — 1 (fast, ~15s) / 2 (balanced, ~30s) / 4 (quality, ~60s)
- **GEMS iterations slider** — how many refine-and-regenerate cycles (1-5)
- **Run Android GEMS** — runs direct generation and GEMS agent loop side by side
- **Gemma 4 Demo** — test the LLM with streaming text output
- **Direct Image Gen Demo** — test image generation standalone

### Run Android GEMS

Shows:
- **Direct Generation** — baseline image from your prompt
- **GEMS Rounds** — each round's image side by side with verification scores
- **GEMS metadata** — refined prompt, final score, total iterations
- **Status updates** — live progress of each agent loop step

---

## Architecture

```
LiteRtLmEngine (Gemma 4, GPU) ──────────┐
                                         ▼
SdCppEngine (SD Turbo, Vulkan GPU) ─► AgentOrchestrator ──► ComparisonScreen
                                         ▼
SkillManager (assets/skills/) ──────► AgentMemory (Room DB)
```

GPU memory is managed by closing one engine before loading the other — the LLM and image generator take turns using the GPU.

## Model Files on Device

After setup, these files should be at `/data/local/tmp/` on the device:

| File | Size | Format | Purpose |
|---|---|---|---|
| `gemma-4-E2B-it.litertlm` | 2.4GB | LiteRT-LM | Gemma 4 E2B multimodal LLM (text + vision) |
| `sd_turbo.gguf` | 1.9GB | GGUF Q8 | SD Turbo image generator (1-4 step distilled, 8-bit quantized) |
| `taesd.safetensors` | 9MB | SafeTensors | Tiny AutoEncoder decoder (10x faster than full VAE) |

## Troubleshooting

- **App crashes on launch**: Make sure `libsdcpp.so` is in `app/src/main/jniLibs/arm64-v8a/`
- **"No SD model found"**: Run `./models/push_to_device.sh` to push models to device
- **Image gen OOM**: Close other apps. The image generator needs ~3GB GPU memory
- **LLM returns empty**: After image gen, the GPU state may be corrupted. The app auto-retries on CPU
- **Second image gen crashes**: The native context is reset between runs to avoid Vulkan state corruption

## Comparison with Original GEMS

### Model Stack

| Component | Original GEMS (Server) | Android GEMS (On-Device) |
|---|---|---|
| **LLM (MLLM)** | Kimi-K2.5 (cloud API) | Gemma 4 E2B (2.4GB, on-device GPU) |
| **Image Generator** | Z-Image-Turbo (cloud API) | SD Turbo Q8 GGUF (1.9GB, Vulkan GPU) |
| **VAE Decoder** | Full VAE (server) | TAESD tiny decoder (9MB, 10x faster) |
| **Skill Routing** | LLM-based routing | Skipped on mobile (saves ~4s) |
| **Max Iterations** | 3 | Configurable 1-5 (default 2) |
| **Verification** | Multimodal (image + text) | Multimodal (Gemma 4 vision input) |
| **Runtime** | Python, multiple GPU servers | Kotlin, single mobile device |

### Agent Loop — Stage by Stage

#### 1. Planner (Skill Router)

**Original GEMS:**
```
Prompt: "You are a strategic Skill Router. Your goal is to determine if the user's
request genuinely requires a specialized skill or if it can be handled by standard
generation. Available Skills: {manifest}. User Request: {prompt}.
Respond ONLY with the SKILL_ID or NONE."
```
If a skill matches, it enhances the prompt using skill-specific instructions.

**Android GEMS:**
Skipped on mobile to save ~4s (2 LLM calls). The original prompt is used directly. Skill routing can be re-enabled for complex prompts.

#### 2. Decomposer

**Original GEMS:**
```
Prompt: "Analyze the user's image generation prompt. Break it down into specific
visual requirements. For each requirement, write a question that can be answered
with a simple 'yes' or 'no'. YOU MUST RESPOND ONLY WITH A JSON ARRAY OF STRINGS.
Example format: ["Is there a cat?", "Is the cat black?", "Is it sitting on a rug?"]"
```

**Android GEMS (identical logic):**
```
System: "You are a requirements agent. Break prompts into yes/no questions. Respond only in JSON."
User: "Analyze the user's image generation prompt. Break it down into specific visual
requirements. For each requirement, write a question answerable with yes or no.
YOU MUST RESPOND ONLY WITH A JSON ARRAY OF STRINGS.
Example: ["Is there a cat?", "Is the cat black?"]

User Prompt: {prompt}"
```

#### 3. Generator

**Original GEMS:** Calls Z-Image-Turbo server API → returns image bytes.

**Android GEMS:** Calls `SdCppEngine.generate(prompt)` → stable-diffusion.cpp loads SD Turbo on Vulkan GPU → runs 1-4 DDIM steps → TAESD decodes latent → returns 512x512 Bitmap. Takes ~15-30s.

#### 4. Verifier

**Original GEMS (parallel, multimodal):**
```
Prompt per question: "Image: <image>
Answer the following question with only 'yes' or 'no' based on the provided image: {question}"
```
Uses ThreadPoolExecutor to verify all questions in parallel with the MLLM.

**Android GEMS (sequential, multimodal):**
```
System: "You are a verification agent. Answer only 'yes' or 'no'."
User: "Look at this image. Answer with ONLY 'yes' or 'no': {question}"
[Image: PNG bytes of the generated image]
```
Runs sequentially (single model instance). Gemma 4 sees the actual image via vision input.

#### 5. Experience Summarizer

**Original GEMS:**
```
Prompt: "Task: Summarize the experience of the current image generation attempt.
--- CURRENT ATTEMPT ---
Prompt used: {current_prompt}
Passed requirements: {passed}
Failed requirements: {failed}
Reasoning/Thought before generation: {current_thought}
Image: <image>
--- PREVIOUS EXPERIENCES ---
{previous_experiences}
--- ANALYSIS ---
Based on the provided image, the verification results, your previous thought process,
and historical experiences, write a concise summary of what worked, what failed, and
what strategy should be adopted in the next attempt. Keep it under 100 words."
```

**Android GEMS (similar, without image in summarizer):**
```
System: "You are a summarization agent. Be concise, under 100 words."
User: "Task: Summarize the experience of the current image generation attempt.
Prompt used: {currentPrompt}
Passed: {passed}
Failed: {failed}
Previous experiences: {prevExpStr}
Write a concise summary under 100 words of what to improve."
```

#### 6. Refiner

**Original GEMS:**
```
Prompt: "Task: Refine the image generation prompt based on previous failed attempts
and accumulated experiences.
Original Intent: {original_prompt}
--- ATTEMPT HISTORY ---
{history_log with <image> tags}
--- ANALYSIS ---
Review the history above. Rewrite a new, comprehensive prompt. This prompt must:
1. Explicitly reinforce the requirements that failed in the latest attempt.
2. Maintain and protect the requirements that were successfully met to avoid regressions.
3. Adopt the strategies suggested in the 'Experience' section.
4. Use clear, non-conflicting descriptive language.
Return ONLY the prompt text itself."
```

**Android GEMS (similar):**
```
System: "You are a prompt refinement agent. Rewrite prompts to fix failures."
User: "Refine the image generation prompt based on previous attempts.
Original Intent: {originalPrompt}
--- ATTEMPT HISTORY ---
Attempt {i}: Experience: {exp}, Prompt: {prompt}, Failed: {failed}
Rewrite a comprehensive prompt that:
1. Reinforces failed requirements.
2. Maintains successful requirements.
3. Uses clear, descriptive language.
Return ONLY the prompt text itself."
```

### Key Differences

| Aspect | Original GEMS | Android GEMS |
|---|---|---|
| **Verification** | Parallel (ThreadPoolExecutor) | Sequential (single model) |
| **Vision in Verifier** | Yes (MLLM sees image) | Yes (Gemma 4 vision) |
| **Vision in Summarizer** | Yes (image passed) | No (text-only summary) |
| **Vision in Refiner** | Yes (history images passed) | No (text-only refinement) |
| **Skill Routing** | Active | Skipped (saves ~4s) |
| **think_with_thought** | Separate reasoning channel | Not available (stripped `<think>` blocks) |
| **GPU Memory** | Multiple server GPUs | Single mobile GPU, engines take turns |
| **Agent Memory** | In-memory trajectory | Room DB persistence + WorkManager compression |

## Credits

- [GEMS](https://github.com/lcqysl/GEMS) — original agent-native multimodal generation paper
- [stable-diffusion.cpp](https://github.com/leejet/stable-diffusion.cpp) — C++ SD inference with Vulkan GPU
- [LiteRT-LM](https://github.com/google-ai-edge/LiteRT-LM) — on-device LLM runtime
- [AI Edge Gallery](https://github.com/google-ai-edge/gallery) — reference for LiteRT-LM integration
