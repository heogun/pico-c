# CLAUDE.md — pico-c

PICO-8(0.2.7) 호환 에뮬레이터. C++17 + SDL2, Lua 런타임은 z8lua(pico8 브랜치,
`lua_Number` = `z8::fix32`). 목표는 **실제 PICO-8과 바이트 단위로 같은 동작**이며,
모든 판단 기준은 오라클(실제 PICO-8) 실측이다.

- 진행 상황 / 로드맵 / 실측 규칙: [progress.md](progress.md) — 새 작업 전에 먼저 읽을 것
- API 구현 현황: [API_CHECKLIST.md](API_CHECKLIST.md)
- 테스트 스위트 설명: [tests/README.md](tests/README.md), 플랫폼: [platforms/README.md](platforms/README.md)

## 빌드 / 테스트

```sh
make PLATFORM=macos                      # → build/macos/pico-c (Makefile이 codesign까지 함)
./build/macos/pico-c tests/carts/test.p8
platforms/macos/test.sh                  # 골든 트랜스크립트 회귀 (PICO-8 불필요)
platforms/macos/test.sh cmp_all.p8       # 카트 하나만
platforms/macos/oracle.sh                # 실제 PICO-8과 직접 비교
platforms/macos/oracle.sh --record       # tests/expected/ 갱신 (오라클 출력으로)
platforms/macos/oracle_audio.sh          # 오디오 스위트(tests/audio)를 PICO-8 .wav export와 비교 (--record로 갱신)
make asan                                # AddressSanitizer 빌드
./build/macos/pico-c -headless -frames 1800 cart.p8   # 벤치마크(프레임당 평균/최악 ms)
./build/macos/pico-c -headless -frames 18000 -autoplay 1 game.p8   # 몽키 테스트(무작위 입력, 오류 탐지)
```

- Windows 네이티브(MSVC)는 CMake: `platforms/windows/build-msvc.bat` / `test-msvc.bat` (SDL2는 vcpkg).
  Windows에 설치된 PICO-8과의 비교는 `platforms/windows/oracle.bat [--record] [cart ...]`(Python 3).
  CMake도 macOS에서 빌드된다: `cmake -S . -B build/cmake -DCMAKE_PREFIX_PATH=/opt/local`.
- 테스트 스위트의 구현은 `tests/tools/run_tests.py` 하나다(Python 3). `test.sh`/`oracle_audio.sh`는 이것을 부른다.
- 첫 빌드 시 `make`(또는 CMake configure)가 z8lua를 `platforms/common/deps/z8lua`로 git clone (네트워크 필요).
- macOS SDL2는 MacPorts(`/opt/local`). amfid 문제로 **ad-hoc codesign 없이는 실행 불가** —
  Makefile이 자동 처리하지만 수동으로 바이너리를 만들면 `codesign --force --sign -` 필요.
- 산출물은 전부 `build/<platform>/` (gitignore).

## 오라클(실제 PICO-8) 사용 시 주의

- `/Applications/PICO-8.app`는 멈춤 → 반드시 사본 사용:
  `cp -R /Applications/PICO-8.app /tmp/p8app && codesign --force --deep --sign - /tmp/p8app`
  (`oracle.sh`가 이 사본을 씀)
- 오라클은 `-x` 헤드리스 실행. 이 모드에서 `save()`는 조용히 무시되고 `info()`는 아무것도 안 찍는다.
- 오라클의 상대 경로 기준은 `~/Library/Application Support/pico-8/carts/`.
- 의도적 차이는 `tests/expected/divergences.txt`(KNOWN), 시계 같은 가변 필드는 `tests/expected/masks.txt`.
- **소리 비교**: `-x`에서는 오디오가 안 돈다(`audio_rec`도 무음). 창 모드(`-windowed 1 -run`) +
  카트의 `export("name_%d.wav")`로 효과음별 .wav(22050Hz)를 뽑는다. 반드시 `-home <임시폴더>`로
  돌릴 것 — 사용자 config가 `sound_volume 0`이라 기본 홈에서는 무음이 나오고, 사용자 설정/카트도
  건드리게 된다(`-volume` 같은 옵션은 config에 저장됨). `oracle_audio.sh`가 이 절차를 따른다.
- 비교는 `tests/tools/wavnotes.py`(창별 피치 판독 + rms/harm/peak/hold 특징값). PICO-8 export는 변화가
  최대 1틱(183샘플) 늦고 시작 지연과 **오실레이터 위상이 실행/효과음마다 달라서** 샘플 단위 비교는
  안 된다(위상을 맞춘 뒤에만 의미 있음). 피치는 창 앞 200샘플을 버리고, 나머지는 허용 오차로 비교한다.

## 소스 구조

| 경로 | 내용 |
|------|------|
| `src/pico8.h` | 우산 헤더 — `pico8_{core,gfx,input,audio,cart,console,vm,charset}.h`를 포함 |
| `src/api/` | Lua API. 서브시스템별 모듈(`api_gfx/text/memory/input/audio/system/cart/lua.cpp`)이 각자 등록, `api_registry.cpp`가 등록 함수 호출. 모듈 간 공유는 `api_internal.h` |
| `src/cart/` | 카트 I/O: `cart_read`(.p8/.p8.png 로드, `#include`, 소스 제한 검사), `cart_write`(.p8), `cart_png`(.p8.png 인코더), `cart_compress`(`:c:`/PXA 해제), `cart_data`, `cart_state`. 내부 헤더 `cart_internal.h` |
| `src/shell/` | 부트 스플래시·콘솔(`console`), 명령 표(`commands.cpp`의 `kShellCommands`), 호스트 파일/폴더(`host`), 키 입력(`input`). 내부 헤더 `shell_internal.h` |
| `src/vm.cpp` | Lua VM, 콜백 호출, 에러 블록 출력, `pico8_apply_parity()`(전역 API 패리티) |
| `src/audio.cpp` | 신디사이저 + 뮤직 시퀀서 + 커스텀 악기(`instrument_note`/`instrument_sample`) + SFX 필터(`channel_sample`) + `.wav` export |
| `src/buzz_table.inc` | buzz 파형 표 (생성기 `tools/gen_buzz.py`, 입력은 PICO-8 .wav export) |
| `src/noise_table.inc` | 키별 노이즈 필터 `{k, m, g}` (생성기 `tools/gen_noise.py`, numpy/scipy 필요) |
| `platforms/common/z8lua.patch` | z8lua에 대한 pico-c의 수정(파서, 숫자 문자열 키, sqrt, 오류 줄 번호) |
| `src/gfx.cpp` `src/input.cpp` `src/memory.cpp` | 화면/입력/RAM의 호스트 측 |
| `src/charset.cpp` + `charset_table.inc` | charset 바이트 ↔ UTF-8 (생성기 `tools/gen_charset.py`) |
| `src/font_table.inc` | 256글리프 비트맵 (생성기 `tools/gen_font.py`, 입력 `tools/font_dump.txt`) |
| `src/pxa.cpp` | 공식 PXA 압축기 이식 |
| `src/tokens.cpp` | 8192 토큰 카운터 (파서와 독립된 렉서) |
| `src/util.{h,cpp}` | 공용 문자열/경로/16진수 헬퍼 |
| `src/main.cpp` | 호스트 루프: 창, 이벤트, 프레임 타이밍, 헤드리스 벤치/autoplay |
| `src/video.cpp` | gif 녹화(`extcmd("rec"/"video")`): 프레임 링 버퍼 + GIF/LZW 인코더 |
| `.github/workflows/test.yml` | CI: Linux/macOS에서 새 체크아웃 빌드 + `test.sh` |

Makefile은 `src/*.cpp`와 `src/*/*.cpp`를 와일드카드로 잡으므로 **새 파일 추가 시 Makefile 수정 불필요**
(단, 하위 폴더는 한 단계까지만).

## 코드 규약

- **모든 함수와 파일 상단에 영어 + 한국어 doc 주석**을 단다 (기존 스타일 그대로: 영어 문단 뒤 한국어 문단).
- 서브시스템 경계를 지킨다: Lua 바인딩은 `src/api/<모듈>`, 카트 포맷은 `src/cart/`, 셸은 `src/shell/`.
  폴더 내부 공유는 `*_internal.h`, 외부 공개는 `src/pico8_*.h`.
- 공용 헬퍼가 필요하면 복사하지 말고 `src/util.h`에 둔다.
- **POSIX 전용 API를 직접 쓰지 말 것**(opendir, stat, getcwd, chdir, mkdir, popen, isatty, localtime_r 등):
  MSVC에 없다. `util.h`의 `util_list_dir`/`util_is_dir`/`util_cwd`/`util_make_dir`/`util_popen`/
  `util_stdin_is_terminal`/`util_local_time` 등을 쓴다. GNU 확장(복합 리터럴 등)도 피할 것.
- 텍스트는 내부적으로 **PICO-8 charset 바이트**로 다루고, 출력/저장 경계에서만 UTF-8로 변환한다
  (`pico8_charset_from_utf8` / `pico8_charset_to_utf8`).
- 셸 명령 추가는 `src/shell/commands.cpp`의 `kShellCommands` 표에 한 줄 추가.
- 동작을 바꿀 때는 먼저 오라클로 실측 → 회귀 카트(`tests/carts/cmp_<주제>.p8`) 추가 →
  `oracle.sh --record`로 골든 기록 → 구현. 실측 규칙은 progress.md에 남긴다.
- 커밋 메시지: `feat:` / `fix:` / `refactor:` / `docs:` 접두어, 영어 본문 + 한국어 한 줄 요약.

## 건드리지 말 것

- `src/stb_image.h`(서드파티), `*.inc`(생성물 — 생성기를 고쳐서 다시 만들 것:
  `gen_font.py`, `gen_charset.py`, `gen_buzz.py`, `gen_noise.py`).
- `platforms/common/deps/z8lua`를 직접 고치면 **반드시** `git -C platforms/common/deps/z8lua diff >
  platforms/common/z8lua.patch`로 패치를 갱신할 것. Makefile이 z8lua를 리비전 `8575dfbc`에 고정하고 이
  패치를 적용한다 — 패치에 없는 수정은 다른 체크아웃에서 사라진다.
- `test_carts/`(저작권 카트), `picotestdriver/`(별도 저장소) — gitignore 대상, 커밋 금지.
- progress.md §4 "이미 잘 동작하는 것들"은 오라클과 일치가 검증된 부분이니 재구현하지 말 것.
