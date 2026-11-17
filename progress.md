# pico-c 진행 상황 & TODO

> 새 세션에서 이어서 진행하기 위한 문서입니다. 가장 최근 갱신: 2026-11-17
> (2026-10-22 소스 구조 리팩터링 반영 — §6. 작업 규약 요약은 [CLAUDE.md](CLAUDE.md))
>
> **파일 경로 주의**: §2~§5는 리팩터링 전에 쓰였다. 거기 나오는 옛 경로는 다음과 같이 읽을 것:
> `src/api.cpp` → `src/api/api_*.cpp`, `src/cart.cpp` → `src/cart/cart_*.cpp`,
> `src/shell.cpp` → `src/shell/*.cpp`, `src/pico8.h`의 선언 → `src/pico8_*.h`,
> `cmp.sh`/`cmp_all.sh` → `platforms/macos/oracle.sh`.

---

## 0. 환경 / 규약 (중요)

- **작업 디렉터리**: `/Users/mumal/Documents/pico-c` (이미 git 저장소)
- **빌드**: `make PLATFORM=macos` (또는 `platforms/macos/build.sh`) → `build/macos/pico-c`
- **코드사인 필수**: 빌드마다 `codesign --force --sign - pico-c`
  (amfid가 깨져있어 안 하면 실행 불가)
- **오라클(실제 PICO-8) 사용법**: `/Applications/PICO-8.app`는 멈춤 → 반드시 사본 사용
  ```
  cp -R /Applications/PICO-8.app /tmp/p8app
  codesign --force --deep --sign - /tmp/p8app
  ```
  실행은 백그라운드 + 출력 리다이렉트, 6~7초 대기 후 강제 종료:
  ```
  ( /tmp/p8app/Contents/MacOS/pico8 -x "$PWD/<cart>.p8" > /tmp/a_out.txt 2>&1 & )
  sleep 6; pkill -f p8app
  grep -v '^RUNNING' /tmp/a_out.txt
  ```
  → 이 과정은 `platforms/macos/oracle.sh`로 자동화됨
- **비교 규칙**: `diff <(grep -v '^DEBUG' ours.txt | sed '/^$/d') <(grep -v '^RUNNING' ref.txt | sed '/^$/d')`
- 오라클의 작업 디렉터리는 `$HOME/Library/Application Support/pico-8/carts/`이므로
  상대 경로 파일 접근은 이쪽에 생기는 걸 주의할 것.
- `z8lua` = `samhocevar/z8lua` **pico8 브랜치** (`make`가 git clone). `lua_Number` = `z8::fix32`.
  변환 헬퍼는 `src/pico8.h`: `fix32_from_int(int32_t)`, `fix32_to_int(fix32)` 등.
  `fix32(double)`, `fix32::frombits(int32_t)`도 있음.

### 0-1. 폴더 구조 (2026-10-22 재편)

```
pico-c/
├── src/                소스 (상세는 §6)
│   ├── api/            Lua API, 서브시스템별 모듈 8개 + api_registry.cpp
│   ├── cart/           카트 읽기/쓰기/PNG/압축 해제
│   ├── shell/          콘솔, 명령 표, 호스트 파일, 키 입력
│   └── *.cpp, pico8_*.h  vm/audio/gfx/input/memory/charset/pxa/tokens/util/main
├── tools/              폰트/문자셋 생성기
├── tests/              테스트 카트 + 골든 트랜스크립트
│   ├── carts/          cmp_*.p8, test.p8, 부속 파일(cstore_base.p8, loadA/B.p8, cmp.p8d)
│   ├── expected/       <cart>.txt 골든 + masks.txt + divergences.txt
│   └── tools/          dumpcart.c
├── platforms/          플랫폼별 의존성 + 빌드/테스트 스크립트
│   ├── common/deps/z8lua/     (make가 클론, 공통)
│   ├── macos/  build.sh|build.bat test.sh|test.bat oracle.sh  deps/(MacPorts SDL2)
│   ├── linux/  build.sh|build.bat test.sh|test.bat             deps/(시스템 SDL2)
│   └── windows/build.sh|build.bat test.sh|test.bat             deps/SDL2-2.30.11(MinGW)
├── build/<platform>/   산출물 (pico-c, obj/, libz8lua.a)
└── Makefile            PLATFORM=macos|linux|windows
```

- `cmp.sh` / `cmp_all.sh`(루트)는 `platforms/macos/oracle.sh`로 대체(단일/배터리/--record 지원).
- `z8lua/` → `platforms/common/deps/z8lua`, `SDL2_mgw/` → `platforms/windows/deps/SDL2-2.30.11`.
- `obj/ obj_asan/ macbuild/ pico-c.exe pico-c-asan dumpcart.exe` → `build/<platform>/`로 이동.
- 골든 트랜스크립트(`tests/expected/*.txt`)는 **실제 PICO-8 출력**을 기록한 것이라
  PICO-8이 없어도 모든 플랫폼에서 회귀 스위트를 돌릴 수 있다.


---

## 1. 완료된 로드맵 (커밋 `e5b08b8`로 GitHub에 push됨)

| 단계 | 항목 | 상태 |
|------|------|------|
| 1 | 카트 I/O (`.p8` writer, `save`/`load`/`reload`/`cstore`) | ✅ |
| 2 | 오디오 정확도 (SFX/뮤직 엔진) | ✅ |
| 3 | 그래픽 `tline`/`map` | ✅ |
| 4 | 시스템 API 갭 | ✅ |
| 5 | 셸 명령 → 전역 Lua 함수 노출 | ✅ (부분) |
| 6 | 토큰/성능 + 숨김/전역 API 패리티 | ✅ (전역 API 패리티 완료, 토큰/성능은 남음) |

### 저장소 상태
- remote: `https://github.com/heogun/pico-c.git` (public, branch `main`)
- commit `e5b08b8` "Add cart I/O, audio engine, tline/map fixes, system API and shell globals"
- 작성자(이 저장소 local 전용): `heogeon <akffod2525@gmail.com>`
- tracked 50 files, `.git` 452K. `.gitignore`로 `platforms/common/deps/z8lua/`,
  `platforms/windows/deps/SDL2-2.30.11/`, `picotestdriver/`,
  `test_carts/`, `build/`, `obj*/`, `macbuild/`, `*.exe`, `tests/carts/test_*.p8`,
  `tests/carts/*.p8d` 제외
- **fresh clone 시**: macOS는 MacPorts `/opt/local`의 SDL2 필요 + `make`가 z8lua 클론(네트워크)

### 핵심 결과물
- (당시) `src/` 11개 파일: `api.cpp`, `audio.cpp`, `cart.cpp`, `gfx.cpp`, `input.cpp`,
  `main.cpp`, `memory.cpp`, `pico8.h`, `shell.cpp`, `vm.cpp`, `stb_image.h`
  → 지금은 폴더별로 분리됨 (§6)
- **회귀 스위트 18/18 오라클 일치** (`tests/carts/cmp_*.p8` + `platforms/macos/oracle.sh`)
  (`cmp_savegen`/`cmp_savereload`/`cmp_pngsave`는 의도적 차이: PICO-8이 `-x`에서 `save()` 비활성화)
- `API_CHECKLIST.md` 갱신됨

---

## 2. 6단계 전역 API 패리티 — 완료 (2026-10-19)

### 2-1. 구현 내용

`src/vm.cpp`의 `pico8_apply_parity(L)` (→ `pico8_vm_init()`에서
`pico8_register_api(L)` **뒤** 호출) 하나에 모두 몰아넣음:

1. **`__traceback` 전역 제거**: 새 헬퍼 `pico8_vm_push_traceback(lua_State*)`
   (`vm.cpp` 정의, `pico8.h` 선언)를 `call_function()`과
   `api.cpp: pico8_menuitem_invoke()`이 사용. 전역으로는 등록 안 함.
2. **코루틴 평탄화** (coroutine 테이블 삭제 전에 복사):
   `cocreate/coresume/costatus/yield` ← `coroutine.create/resume/status/yield`
   (wrap은 PICO-8에 없음)
3. **제거**: `_G _VERSION __pico8_time __traceback collectgarbage coroutine
   debug dofile error loadfile pcall string table tonumber xpcall`
   + 등록 삭제: `seconds`, `resume` (`api.cpp`의 정의도 삭제).
   셸 명령 `resume`은 `shell.cpp`에 따로 있어 그대로.
4. **추가**:
   - `inext(t, i)`: 기본 `i=0`, `t[i+1]`이 nil이면 **아무것도 안 돌려줌**,
     아니면 `i+1, t[i+1]` (구멍을 건너뛰지 않음)
   - `__type` = `type` 별칭
   - 글리프 상수 26개 (값은 2-4 표, 키는 정확한 UTF-8 바이트를 octal 이스케이프로)
   - 숨김 스텁 16개 (`__flip __trace _get_menu_item_selected _map_display
     _mark_cpu _menuitem _set_fps _set_mainloop_exists _startframe
     _update_buttons _update_framerate holdframe mapdraw scoresub
     set_draw_slice trace`) → `return 0` (**nil이 아니라 0개 반환**.
     그래서 `tostr(_set_fps(30))`이 빈 문자열)
   - `_pausemenu` → 빈 테이블

### 2-2. 검증 결과 (전부 오라클 일치)

- `cmp_globlist.p8`: 이름 **집합** 176 = 176, `comm -13`/`comm -23` 모두 **0**
  (순서 차이는 §5의 캐릭터셋 표현 차이 때문 — 유일한 남은 차이)
- 신규 매칭: `cmp_tostr` `cmp_inext` `cmp_internal` `cmp_globals` `cmp_pcall`
  `cmp_glyph` `cmp_glyphkey`
- 회귀 29종 매칭: `cmp_all cmp_audio_music cmp_audio_stat cmp_cstore cmp_dget
  cmp_rrect cmp_songmem2 cmp_stat cmp_tline cmp_tline_mask cmp_tline_px
  cmp_tline_layers cmp_tline_my cmp_tline_my2 cmp_bgfill cmp_map_layers
  cmp_info cmp_menuitem cmp_extcmd cmp_printh cmp_dump cmp_dumpresave
  cmp_inext cmp_internal cmp_globals cmp_pcall cmp_tostr cmp_glyph
  cmp_glyphkey`
- 주의: `cmp_stat`은 `stat(84)`/`stat(94)`가 초 경계에서 1초 차이 나는
  **플레이크** — 재실행하면 통과.
- 도우미 스크립트: `./cmp.sh <cart.p8>` (단일), `./cmp_all.sh <cart...>` (배터리).
  둘 다 `/tmp/p8app` 사본을 오라클로 사용하고 `LC_ALL=C`로 바이트 비교.

### 2-3. tostr() 조사 (3-1 완료)

`tostr()`(인자 없음) = 빈 문자열, `tostr(f())`(빈 반환) = 빈 문자열,
`tostr(nil)` = `[nil]`, `tostr(false)` = `false`, `tostr({})` = table repr —
**우리 구현과 이미 동일** (`cmp_tostr` MATCH). 즉 "빈 반환"이 빈 문자열로
출력되는 현상은 tostr이 인자를 못 받은 것.

---

## 3. TODO (다음 세션 시작 지점)

### 3-1. 글리프 캐릭터셋 표현 패리티 — [x] 완료 (2026-10-19, 상세는 §5)

PICO-8은 텍스트를 **charset 바이트**로 저장하고 경계에서만 UTF-8로 변환한다.
우린 완전히 동일하게 구현 완료 — 회귀 배터리 38종 전부 MATCH
(`cmp_charset`, `cmp_printwidth` 추가).

구현 (z8lua 패치 불필요했음 — `luai_ctype_` 테이블이 0x80..0xFF를 이미
알파벳으로 표시하고 있어서 렉서가 글리프 식별자를 그대로 받음):
1. `src/charset.cpp` + `src/charset_table.inc` (생성기 `tools/gen_charset.py`,
   오라클 chr()/printh() 측정값 256항목):
   - `pico8_charset_from_utf8()` — 로드 시: 알려진 UTF-8 글리프 시퀀스(157개,
     longest-match) → 단일 바이트, 모르는 문자(`한`,`😀`)는 raw UTF-8 바이트 유지
   - `pico8_charset_to_utf8()` — 출력/저장 시: 바이트 → UTF-8 글리프,
     ASCII(9,10,13,32..126) 통과, **NUL에서 잘림** (printh의 C 문자열 시맨틱)
2. `src/cart.cpp`: 두 로더 모두 소스를 `from_utf8` 변환 (`g_cart_source` =
   내부 바이트, 포함 파일도 최종 텍스트에서 1회 변환), writer의 `__lua__`
   섹션은 `to_utf8`로 재변환 → **cstore 출력 파일이 오라클과 바이트 동일**
3. `src/api.cpp` `pico8_printh`: 콘솔/파일/클립보드 출력 전부 `to_utf8`
4. `src/vm.cpp`: 에러 블록(§3-2)은 원문 바이트 그대로 출력, 콘솔 입력
   (`pico8_vm_eval`)은 `from_utf8`, **글리프 상수 26개를 charset 바이트 이름으로 등록**
   (UTF-8 이름 그대로 두면 카트의 바이트 식별자를 못 찾고 출력도 깨짐)
5. `src/api.cpp` `pico8_ord` 신규 (z8lua版을 그림자): 오라클 시맨틱 —
   범위 밖/비문자열 인자 → **nil 반환** (z8lua版은 값 0개), count<1 → 값 0개,
   숫자는 문자열 강제변환(`ord(5)`=53)
6. `print()` 폭: 바이트 128..255 = **8px**, 16..127 = 4px, 0..15 = 0px
   (tab 16px, \n/\r 커서 이동 — 전부 `print(chr(c))` 실측)

남은 차이는 charset이 아니라 별도 항목:
- [x] **화면 글리프 렌더링** — 완료 (2026-10-20): 오라클 화면 RAM(0x6000)을 `peek()`으로
  덤프해 256글리프 비트맵을 추출(`tools/font_dump.txt` + `tools/gen_font.py` →
  `src/font_table.inc`). 16..31 = 3×5(4px 전진), 128..255 = **7×5 실제 비트맵**
  (8px 전진, 2배 확대 아님), 1..15는 제어 코드(글리프는 5만 잉크), 0은 문자열 종료.
  `pico8_draw_char`가 코드에 따라 표를 고름. 회귀 `cmp_screen`(화면 RAM 바이트 비교,
  홀수 좌표·가장자리 클리핑 포함) MATCH
- [x] **런타임 에러 형식 패리티** — 완료 (§3-2): stdout 4줄 블록(프레임 포함),
  `cmp_glyphbytes`/`cmp_glyphord`도 이제 MATCH

### 3-2. 작은 것들
- [x] **p8scii / print 제어 코드** — 완료 (2026-10-20). `src/api.cpp`
      `print_state`/`print_walk` + `src/gfx.cpp` `pico8_draw_char_text`,
      회귀 `cmp_p8scii` MATCH. 실측한 규칙:
      - **전경/배경색**: `\f` + **소문자 hex digit**(`0-9a-f`; 대문자·그 외는 0=검정).
        배경색은 셀을 채우고, 그 셀은 `(x-sx, y-sy)`에서 폭 `진행폭*sx`,
        높이 `6*sy` (sy=1이면 7행, 2면 14행) — 글리프보다 위/왼쪽으로 1px 크다.
      - **모드**: `\^i` 반전, `\^w` 2배폭, `\^t` 2배높이, `\^p` 핀볼(2배 간격 1px 점,
        x+1 오프셋, 전진 8), `\^-w`/`\^-t`/`\^-i`/`\^-p` 해제, 모르는 글자는 무시.
        반전은 배경을 전경색으로 채우되 **글리프 잉크 자리는 비워 두고**, 잉크는
        배경색으로(배경색이 없으면 스크린 유지).
      - **반복**: `\*` + hex digit + 다음 1바이트. 0/잘못된 digit이면 0회(그 바이트는 소비).
        반복 대상이 제어 바이트면 1회만 처리.
      - **커서**: `\b` x-=4, `\t` 다음 16배수, `\n` x=시작x·y+=6, `\r` x=시작x.
        `\a`(7)·11·14는 인자 1바이트를 먹고 **문자열 종료**. 15는 자기 글리프(4px)를
        그리고 인자 1바이트를 먹음(인자 없으면 아무것도 안 함).
      - **미확인 커서 op**: `\-`(3)은 `x = x-16+digit`, `\|`(4)는 `y = y-16+digit`,
        `\+`(5)는 인자 1바이트 먹고 종료. 문서에 없는 내부 코드라 인자가 digit이 아닐 때는
        오라클과 다를 수 있음(카트에서 거의 안 씀). `\^`+digit은 오라클이 print에서
        nil을 반환/멈추는 병리 케이스라 손대지 않음.
      - **print 인자**: `print(s, col)`(2개)은 커서 위치에서 색만 바꿈(이전엔 x로 오해).
        print는 끝나면 `0x5f25`(색)←마지막 전경색, `0x5f26`←시작x, `0x5f27`←마지막 y+6.
- [x] **런타임/문법 에러 출력 형식** — 완료 (2026-10-20)
      오라클(0.2.7)은 카트 에러를 **stderr 없이 stdout**에 블록으로 출력:
      ```
      runtime error line 24 tab 0
      <소스줄 원문 — 변환 안 된 charset 바이트 (0x95 그대로, 깨진 UTF-8)>
      attempt to compare number with string
      in inner line 2 (tab 0)          <- Lua 프레임마다, 안쪽부터
      at line 7 (tab 0)                <- 가장 바깥 프레임(=메인 청크)
      ```
      문법 에러는 3줄 `syntax error line N (tab 0)` / 소스줄 / 메시지.
      구현: `src/vm.cpp`의 `runtime_error_block()`(traceback 핸들러) + `error_position()`
      + `chunk_line()`(로드한 청크 텍스트를 `g_chunk_text`에 보관), `pico8_vm_load_buffer`
      및 `call_function`이 블록을 그대로 stdout에 씀. 세부 규칙(실측):
      - 첫 줄은 `line <N> tab 0`, 이후 프레임도 `(tab 0)` — 깊은 들여쓰기(16칸/탭)도 0
      - 엔진이 직접 호출한 함수는 메인 청크 프레임이 없어서 마지막 줄이
        `_init`이면 **`at line 1 (tab 1)`**, `_draw`/`_update`/`_update60`이면
        **`at line 0 (tab 0)`** (호출 이름은 `call_function`이 `g_entry_name`으로 전달)
      - C 함수 프레임은 표시 안 함, `in <이름>`은 호출된 이름(`local f=inner; f()`도 `in f`)
      - 프레임 콜백 에러 시 카트 **정지**(오라클은 에러를 프레임마다 반복 출력하지 않음)
        → `pico8_vm_halted=1`
      - 로드 실패 시 `main.cpp`의 `Failed to load cart`는 생략(`pico8_vm_error_reported()`)
      - 남은 차이: **문법 에러의 메시지/위치**는 파서 의존(z8lua: `unexpected symbol near
        '<eof>'` line 5 vs 오라클: `unclosed (` line 4) — 형식은 동일, 문구만 다름
      회귀: `cmp_errorframe`, `cmp_errorinit`, `cmp_glyphord`, `cmp_glyphbytes` 추가 → 전부 MATCH.
- [x] `src/cart.cpp`: 카트 헤더 버전 → **항상 43** (`kCartVersion`).
      근거: 오라클(0.2.7)은 불러온 카트의 버전(41)을 무시하고 cstore() 결과에
      항상 자기 버전 43을 씀. 또 우리 writer의 출력은 오라클 출력과
      **버전 줄 말고 바이트 단위로 동일** (`cmp_cstore` 결과를 직접 diff로 확인).
      그래서 `cart_parse_version()`/`pico8_cart_get_version()`은 제거.
      부수 효과: `cmp_cstore` 실행해도 `cstore_base.p8`이 더는 git diff에 안 남음.
      (참고: `save()`는 `-x` 실행에서 PICO-8이 조용히 무시함 — 파일이 안 생김)

### 3-3. 회귀 확인 방법
- `platforms/macos/oracle.sh <cart...>` — 오라클(PICO-8 사본)과 우리 구현을 비교.
  `--record`를 주면 `tests/expected/<cart>.txt` 골든 트랜스크립트를 갱신한다.
- 표준 배터리: `platforms/macos/oracle.sh` (tests/carts/cmp_*.p8 전체)
  → 현재 **43 MATCH + 2 KNOWN**(KNOWN은 의도적 차이)
- PICO-8이 없어도 되는 순수 회귀: `platforms/<plat>/test.sh`
  → 골든 트랜스크립트(`tests/expected/`)와 비교, 어느 플랫폼에서나 동작
- `cmp_stat`은 시계 값(`stat(80..85)` UTC / `stat(90..95)` local)이 실행 시각에 따라
  변해서 **플레이크**가 된다 — 초는 1초, 분은 1분, 시는 1시간, 일은 자정마다 바뀐다
  → `tests/expected/masks.txt`에서 시계 필드 전체를 마스크 처리
  → 대신 `tests/carts/cmp_stat_time.p8`가 "각 필드가 항상 올바른 범위에 있는가"만
    검사하므로 시각과 무관하게 시계 동작을 검증한다
- 제외 2종: `cmp_savegen` / `cmp_pngsave` — `save()`는 `-x` 실행에서 PICO-8이
  조용히 무시해서 파일 쓰기를 비교할 수 없음
  (`tests/expected/divergences.txt`에 명시)
- `cmp_glyphord`의 글리프 전역 `pairs()` 순서는 **해시 순서(구현 정의)**라
  카트에서 정렬한 뒤 비교한다 (집합 자체는 이전부터 일치).

### 3-4. 토큰 카운트 / 소스 크기 제한 (2026-10-20 오라클 실측)

PICO-8은 카트의 `__lua__` 소스가 **65536바이트를 넘거나 토큰이 8192개를 넘으면**
실행하지 않는다. 두 제한의 경계와 메시지를 `/tmp/p8app` 사본 + `-x`로 실측했다.

- **소스 크기**: 65536 이하는 정상, **65537 이상**이면 `could not load <절대경로>`
  한 줄. 토큰 제한보다 **먼저** 검사한다(둘 다 넘으면 `could not load`가 이김).
- **토큰**: 8192 이하는 실행, **8193 이상**이면 카트는 로드되지만 실행되지 않고
  ```
  program too large
  10136 / 8192 tokens
  ```
  를 출력(끝에 빈 줄 하나). `info()`는 헤드리스 `-x`에서 아무것도 내보내지 않아서
  이 메시지가 토큰 수를 읽는 유일한 통로다 → `pico8_info()`는 건드리지 않는다.

구현:
- `src/tokens.cpp`의 `pico8_count_tokens()`: 전체 소스를 **파싱 전에** 한 번
  렉싱하는 관용적 토크나이저. 제한 검사가 렉서/파서 에러보다 먼저 걸리므로(위치
  무관 실측) z8lua 파서에 얹지 않고 독립적으로 돌린다. 규칙 전체는 파일 상단
  주석에 정리해 뒀다.
- `src/cart.cpp`의 `check_source_limits()`: `load_p8_text`/`load_p8png` 두 로더가
  `#include` 처리와 charset 변환을 마친 텍스트를 검사한다. 초과 시
  `PICO8_LOAD_TOO_LARGE`를 돌려주고 `src/main.cpp`가 `could not load %s` 또는
  `program too large` + `%d / 8192 tokens`를 같은 형식으로 출력한다.
- 부수 수정: 두 로더의 고정 `char line[4096]`을 동적 버퍼로 교체(긴 줄이 더는
  잘리지 않음).
- `cmp.sh`/`cmp_all.sh`가 오라클과 **같은 절대 경로**(`"$PWD/$cart"`)를 넘긴다.

실측한 렉서 규칙(모두 1토큰 기준, 전부 오라클 대조):
- **무료(0)**: `) ] } , ;`, 주석(`--`, `//`, `--[[ ]]`, `--[==[ ]==]`), 키워드
  `end` `local`, 그리고 **`.`과 `:`** — 그래서 `a.b`, `a . b`, `a : b()`,
  `::lbl::`은 쓰인 이름 개수만 센다. `..`, `...`, `..=`는 연산자 1개.
- **숫자는 자기 알파벳에 탐욕적**: `1..2`/`1.5..2`/`0x1.5`=1, `1e5`=2, `0x1_f`=2,
  `1_0`=2. (`x=1..2`는 토큰 1개지만 컴파일 에러 — 카운트는 그 에러를 견딘다.)
- **단항 `-` + 숫자 병합**: 앞 토큰이 식을 끝낼 수 없을 때만, 그리고 뒤가 진짜
  숫자일 때만.
  - 병합함: `= ( [ { ,`와 모든 연산자, `and or not if while until return elseif`
  - 병합 안 함: 이름/숫자/문자열/`) ] }`, `#`, `..`, 그리고 문장을 여는 키워드
    `then do else end local repeat for in goto` (예: `if -1 then -1=1 end`는 7토큰)
  - 뒤가 숫자여야 함: `-.5`는 병합, `-...`는 `-` + `...`로 2토큰
- **최장 일치 연산자**: `>>>=`, `<<= >>= ^^= ..=`(각 1), `&&`/`~~`/`<>`=2, `!`=1.
- `bnot`/`band`/`shl`/`shr` 등 비트 연산은 그냥 이름(각 1).

회귀: `cmp_tokenlimit.p8`(규칙별 라인 ×30 → 10622토큰, 양쪽 동일),
`cmp_srclimit.p8`(65544바이트 Lua → 양쪽 `could not load <절대경로>`).
표준 배터리는 **40/40 MATCH**. 그 밖에 256개 스니펫을 한 카트에 몰아넣은 합산
대조와 무작위 토큰열 퍼즈 15카트를 오라클과 대조해 전부 일치했다.

### 3-6. `.p8.png` 저장 (2026-10-20)

참고 자료: Lexaloffle이 공개한 공식 압축기 `dansanderson/lexaloffle`
(`pxa_compress_snippets.c`, `p8_compress.c`)와 위키 `P8PNGFileFormat`.

**파일 구조** (160×205 RGBA8 = 32,800 = 0x8020 바이트)

- 카트 바이트를 각 채널(R,G,B,**A**)의 하위 2비트에 스테가노그래피로 저장:
  `byte = b | (g<<2) | (r<<4) | (a<<6)` (A가 최상위 2비트)
- `0x0000..0x42ff` 데이터 영역, `0x4300..` 코드 영역, `0x8000..0x801f` 메타데이터
  (버전 스탬프 `VV 00 02 07 6d 00` + SHA1 20바이트, 다운로드 카트는 전부 0)
- 코드 영역: `\0pxa`(압축) / `:c:\0`(레거시) / 그 외는 NUL까지 평문

**PXA 압축기** (`src/pxa.cpp`, `src/pxa.h`)

- 공식 C 코드를 C++로 이식. 알고리즘·상수·정수 나눗셈 점수 계산을 그대로 유지
  (`HASH_MAX 4096`, `MINI_HASH`, 3/3/5/4 비트 배분, score `i*256/bit_cost`,
  raw 블록 재작성 0.2.0j 로직). 유일한 변경: `codo_malloc` → `std::vector`,
  정적 해시 테이블 → 함수 지역 상태, `char *modified_code` → `std::vector`.
- **검증**: 실카트(`coloro`, `rose_king`)의 압축 스트림이 PICO-8 파일과
  **바이트 단위로 동일** (4190 / 5226바이트). 8개 카트 왕복 + 멱등성 확인.

**PNG 인코더** (`cart_write_p8png`, `src/cart.cpp`)

- 자체 구현: IHDR(160,205,8,6) + IDAT(stored deflate, zlib/adler32) +
  `tEXt "LodePNG\00020110221"` + IEND. CRC32 테이블 내장.
- 라벨 없는 카트는 검정(0,0,0) 픽셀 + 알파 하위비트로 데이터 표현.
- 0x8020 페이로드는 실카트와 **바이트 동일** (blobbuild/blobcmp로 확인).
  PNG 컨테이너 바이트는 LodePNG와 다름(무압축 zlib).

**연결**

- `pico8_cart_save_png` / `pico8_cart_save_rom_png` (pico8.h + cart.cpp)
- `pico8_save`(api.cpp): `.png` 확장자 → `pico8_cart_save_png`
- `pico8_cart_cstore`(cart.cpp): `.png` → `cart_write_p8png`
- `g_cart_logo[32]`: 로드한 카트의 0x8000..0x801f 보존 → 저장 시 그대로 기록

**검증**

- 오라클이 우리가 쓴 `.p8.png`를 읽고 실행함 (여러 크기: 평문/압축 경계 포함)
- 우리 로더도 자기 파일을 다시 읽음
- 코드 영역 초과 시 `cart code does not fit the .p8.png code area` 후 파일 미생성
- 글리프 소스는 charset 바이트로 저장 (PICO-8과 동일)
- 표준 배터리 **40/40 MATCH** 유지

### 3-5. 이후 남은 로드맵
- [x] **토큰 카운트 / 소스 크기 제한** — 완료 (§3-4): `src/tokens.cpp`의 독립
      토크나이저 + `check_source_limits()`로 8192토큰/65536바이트 제한과 두 메시지
      까지 오라클과 일치. `info()`의 콘솔 출력만 헤드리스에서 비교할 수 없어 그대로 둠.
- [x] **`.p8.png` 저장** — 완료 (§3-6): 공식 PXA 압축기 이식 + 자체 PNG 인코더.
      실카트 페이로드가 PICO-8 출력과 **바이트 동일**.
- [x] **소스 구조 리팩터링 + 이중 언어 문서화** — 완료 (2026-10-22, §6)
- [x] **리팩터링 브랜치 병합** — 완료 (2026-11-17): PR heogun/pico-c#1을 merge commit
      `e1025e4`로 main에 병합(커밋 해시 보존). 참고: `62c20a3`/`1175c0b`는 메시지가 같지만
      두 번째는 일회용 `tools/split_*.py` 삭제 커밋이다.
- [x] **README 갱신** — 완료 (2026-11-17): "Source layout" 표를 `src/api|cart|shell/` 구조로,
      "Console" 절에 `edit`/`menu`/`man` 추가 및 미구현 명령 목록 정정
- [x] **커스텀 악기 (파형 8–15)** — 완료 (2026-11-17, §7)
- [x] **SFX 필터** (byte 64: noiz/buzz/detune/reverb/dampen) — 완료 (2026-11-17, §8)
- [x] **일반 노이즈 (파형 6)** — 완료 (2026-11-17, §9-1)
- [x] **음 타이밍 / 효과 계단** — 완료 (2026-11-17, §9-2). PICO-8 export의 시작 지연(실행마다 다름)만 미재현
- [x] **성능 + z8lua 패치 관리** — 완료 (2026-11-17, §9-3)
- [ ] 5단계 마무리 (셸 명령):
  - [x] `edit` — `$EDITOR`(없으면 `vi`)로 카트 소스를 열고 종료 후 다시 로드 (커밋 `2dab3fd`).
        PICO-8식 내장 코드/스프라이트/맵/SFX 에디터는 없음 (큰 작업, 범위 결정 필요)
  - [x] `export foo%d.wav` / `export foo.png`, `import foo.png` (§7, §9-4). html/bin은 없음
  - [~] `keyconfig` — 안내 메시지만. 버튼 리맵 저장 미구현
  - [~] `splore` — 온라인 브라우저는 범위 밖, 로컬 `load` 안내만
- [x] **문법 에러 문구 패리티** — 완료 (2026-11-17, §9-4)
- [x] **extcmd 캡처, serial** — 완료 (2026-11-17, §9-4). `label`, gif는 §10에서 완료
- [x] **CI, 실제 게임 호환성, 라벨/gif, 경고 정리** — 완료 (2026-11-17, §10)
- [x] **잠재 버그 수정** (셸 카트 상태, import) — 완료 (2026-11-17, §11)
- [x] **Windows 네이티브(MSVC) 빌드 + CI** — §12
- [ ] (방해 요소) `/Applications/PICO-8.app` 자체가 멈춤 — /tmp/p8app 사본 사용 유지

---

## 4. 이미 잘 동작하는 것들 (재구현 금지)

- `.p8` writer: 헤더 `version 43` (오라클과 바이트 동일), 섹서 순서 `__lua__ __gfx__ __gff__ __label__ __map__ __sfx__ __music__`,
  gfx는 low-nibble-first, gff는 MSB-first, 맵 MSB-first, sfx는 64줄(헤더 4바이트 + 32노트),
  music 64줄 `FF c0c1c2c3`
- SFX RAM: `0x3200`에서 노트당 2바이트, 한 레코드 68바이트. `PICO8_SFX_SIZE=68`,
  `pico8_init_sfx_default()`가 sfx0=1, 나머지=16 속도 지정
- 뮤시카 default `0x41,0x42,0x43,0x44` (song RAM)
- `LAYERS`는 **any-bit** 매치 (`(flags & layer) == 0` → skip), `layer=0`이면 전체 그림
- `tline`은 마스크(0x5f38/0x5f39) → **그 다음** 오프셋(0x5f3a/0x5f3b)
- `map()`은 배경 채움 `POKE(0x5f36,0x8)` 준수
- 헤드리스는 오디오 콜백이 없으니 `stat(50..53)`을 ch offset에서 파생
- `printh` 기본 append / `overwrite` truncate / `@clip` → pbcopy / `save_to_desktop` → ~/Desktop
- `extcmd`: reset/shutdown/pause/folder/go_back 동작, 나머지 무시
- 셸 명령 전역 함수: `dir cd mkdir help backup exit export import install_demos
  install_games keyconfig login shutdown splore` (api.cpp `kShellFuncs`)

---

## 5. 글리프의 내부 표현 (2026-10-19 오라클 실측 — §3-1의 근거)

**PICO-8은 소스의 글리프를 단일 charset 바이트로 저장한다.** (`cmp_glyphord.p8`)

- `g="⬅️"` → `#g` = **1**, `ord(g,1)` = **139**, `sub(g,1,1)` = "⬅️"
- `g=="\226\172\133\239\184\143"` → **false**, `#"\226\172\133\239\184\143"` = **6**
  → 즉 리터럴 문자(`⬅️`)는 변환되지만 `\ddd` **이스케이프는 변환되지 않음**
  ⇒ 변환이 렉서(문자 단위 디코딩)에서 일어남
- 글리프 코드 표 (전역 키의 `ord(k,1)` 실측, `cmp_globlist` 출력 순서와 정확히 일치):

  | 코드 | 글리프 | 코드 | 글리프 | 코드 | 글리프 | 코드 | 글리프 |
  |------|--------|------|--------|------|--------|------|--------|
  |128|█|129|▒|130|🐱|131|⬇️|
  |132|░|133|✽|134|●|135|♥|
  |136|☉|137|웃|138|⌂|139|⬅️|
  |140|😐|141|♪|142|🅾️|143|◆|
  |144|…|145|➡️|146|★|147|⧗|
  |148|⬆️|149|ˇ|150|∧|151|❎|
  |152|▤|153|▥| | | | |

- 출력(printh/`..`→stdout)은 charset 바이트를 **UTF-8로 재변환**해서 씀
  (그래서 덤프에는 UTF-8로 보임)
- 문자열 비교는 **내부 표현(=charset 바이트) 기준**. ASCII는 양쪽 모두 1바이트
  같은 값이라 비교 결과가 동일 (`"add"<"Zed"` false, `"Apple"<"zebra"` true,
  `"a"<"B"` false → 바이트 순서)
- 글리프 상수(전역 변수)의 **값**은 이 코드와 무관:
  ```
  ⬅️=0  ➡️=1  ⬆️=2  ⬇️=3  🅾️=4  ❎=5  █=0.5
  ˇ=-2624.5  …=-2560.5  ∧=31455.5  ⌂=-20192.5
  ░=32125.5  ▒=23130.5  ▤=3855.5   ▥=21845.5
  ◆=-20032.5 ●=-1632.5  ★=-20128.5 ☉=-19008.5
  ♥=20927.5  ♪=-25792.5 ✽=-18402.5 ⧗=6943.5
  🐱=20767.5 😐=-24351.5 웃=-26208.5
  ```
  (`cmp_glyph.p8`이 `tostr()` 실측값 기준으로 오라클과 일치)
- **`cmp_globlist` 순서 차이의 원인**: 오라클의 글리프 정렬은 charset 코드
  오름차순(128→153)이고 우리는 UTF-8 바이트 정렬이라 다르다. 이름 집합은 일치.

### 조사용 카트
- `cmp_glyphord.p8` — `#`, `ord`, `sub`, `==`, 키 코드 덤프, ASCII 비교
- `cmp_glyphbytes.p8` — 키의 raw 바이트 덤프

---

## 6. 소스 구조 리팩터링 (2026-10-22, 브랜치 `refactor/split-api-and-document`)

동작 변경 없이 구조만 바꾼 작업. 각 단계 후 회귀 스위트 통과를 확인했다.
최종 확인(2026-11-17, `1175c0b`): `platforms/macos/test.sh` → **45 passed, 0 failed**.

| 커밋 | 내용 |
|------|------|
| `f07f27b` | 테스트를 `tests/`로, 빌드 의존성을 `platforms/`로. `PLATFORM=` 빌드, `build/<platform>/` 산출물 |
| `07016d1` | `src/api.cpp`(2251줄) → `src/api/` 모듈 8개 + `api_registry.cpp` |
| `5adc552` | `src/pico8.h` → 서브시스템 헤더 8개(`pico8_{core,gfx,input,audio,cart,console,vm,charset}.h`) + 우산 헤더. Makefile에 `-MMD -MP` |
| `63f7742` `474933f` `fc06eea` `68e295d` | 모든 함수/파일에 영어+한국어 doc 주석. 죽은 코드(`pico8_blit_char`, `tools/split_api.sh`) 제거 |
| `75d7cd6` | README에 소스 구조 표 |
| `f7c05c3` | 경로/16진수 헬퍼를 `src/util.{h,cpp}`로 통합 |
| `f19d042` | `src/shell.cpp`(1517줄) → `src/shell/` 4개 파일, 명령 디스패치를 `kShellCommands` 표로 |
| `62c20a3` `1175c0b` | `src/cart.cpp`(1486줄) → `src/cart/` 6개 파일 + `cart_internal.h`, 일회용 분리 스크립트 삭제 |

현재 `src/` (stb_image.h 제외 약 12,700줄):

```
src/
├── pico8.h, pico8_*.h        공개 헤더 (우산 + 서브시스템 8개)
├── api/                      api_{audio,cart,gfx,input,lua,memory,system,text}.cpp
│                             api_registry.cpp, api_internal.h
├── cart/                     cart_{read,write,png,compress,data,state}.cpp, cart_internal.h
├── shell/                    console.cpp, commands.cpp, host.cpp, input.cpp, shell_internal.h
├── vm.cpp audio.cpp gfx.cpp input.cpp memory.cpp main.cpp
├── charset.cpp + charset_table.inc, font_table.inc
├── pxa.cpp/h, tokens.cpp, util.cpp/h
└── stb_image.h               (서드파티)
```

가장 큰 파일: `api/api_gfx.cpp`(1375줄), `audio.cpp`(961), `shell/commands.cpp`(882),
`vm.cpp`(788). 추가 분리가 필요하면 이 순서로 검토.

---

## 7. 커스텀 악기 (2026-11-17)

### 7-1. 오라클로 소리 비교하는 법 (새로 확보)

- `-x` 헤드리스에서는 오디오 믹서가 돌지 않는다: `extcmd("audio_rec")`도 빈 .wav, Lua `export()`는 멈춤.
- **창 모드 + Lua `export("name_%d.wav")`**가 효과음 64개를 각각 .wav로 쓴다
  (22050Hz, 16비트, 모노, 길이 = 32음 × speed × 183샘플, 삼각파 vol 7의 정점 16122).
- **`-home <임시폴더>` 필수**: 사용자 config에 `sound_volume 0`이 있어 기본 홈에서는 export가
  전부 무음이었다. `-volume 256` 같은 커맨드라인 값은 config에 저장되니 사용자 홈으로 돌리지 말 것.
  (이번 측정 중 사용자 config의 `volume`이 `-volume` 실행으로 256이 되었다 — 원래 값은 미확인)
- 자동화: `platforms/macos/oracle_audio.sh [--record]`, 판독기 `tests/tools/wavnotes.py`.

### 7-2. 실측 규칙 (PICO-8 0.2.7)

- 파형 8..15인 음 = 효과음 0..7을 악기로 연주. **키 24가 원래 높이**(피치 = 악기 음 + 바깥 음 - 24),
  **음량은 곱**, 악기는 **자기 speed**로, 자기 loop/효과대로 돈다.
- 바깥 음의 효과는 악기 위에 얹힌다: slide/vibrato/arp는 주파수 비율로, fade는 음량으로.
  **drop은 피치를 떨어뜨리지 않고 재트리거만** 한다.
- **재트리거(악기 시계를 0으로)**: ① 이전 음과 다른 악기(이전 음이 일반 파형이었던 경우 포함),
  ② 피치가 바뀜 — 단 **slide 음은 제외**(레가토), ③ 효과 drop(3), ④ 효과 fade in(4).
  그 외(같은 피치, 음량만 변경, fade out, vibrato, arp)에는 시계가 이어진다.
- 판정은 **음량 0인 음에도** 적용된다: 음량 0 + 피치 변경이면 재트리거, 음량 0 + 같은 피치면
  시계가 그 동안에도 흐른다. 첫 음이 음량 0이어도 거기서 시계가 시작한다.
- 루프 없는 악기는 끝(음량 있는 마지막 음 다음)에 도달하면 다음 재트리거까지 무음.
- 악기 안의 사용자 정의 음은 **중첩되지 않고** 같은 번호의 기본 파형(0..7)으로 연주된다.
- 매뉴얼의 "이전 음의 음량이 0이면 재트리거"는 0.2.7 실측과 다르다(위 ③④와 피치 규칙으로 설명됨).

### 7-3. 구현

- `src/audio.cpp`: `struct Instrument`(채널별 악기 시계), `instrument_note()`(재트리거 규칙),
  `instrument_sample()`(악기 진행 + 내부 효과), 공용 `note_effect()`/`sfx_loop()`/`sfx_last_note()`.
  새 음 판정은 `floor(Channel::time)` 변화로 한다(루프로 같은 음 번호가 돌아와도 새 음).
- `.wav` export: `pico8_audio_export_sfx_wav()` + 셸/Lua `export foo%d.wav`(전부) / `export foo.wav`(sfx 0).
- 회귀: `tests/audio/carts/inst.p8` + `tests/audio/expected/inst.txt` 24케이스, `test.sh`에 포함
  (python3 없으면 SKIP). 전체 **69 passed**(트랜스크립트 45 + 오디오 24). 오라클 2회 연속 24/24.
- 테스트에서 뺀 케이스(타이밍 의존): 악기 위 slide 곡선, vibrato 끝 꼬리, fade 음량 값.

### 7-4. 다음: SFX 필터

- byte 64 인코딩(위키 기준, 미검증): bit0 편집 모드, bit1 noiz, bit2 buzz,
  detune = (v/8)%3, reverb = (v/24)%3, dampen = (v/72)%3.
- 같은 방법으로 필터별 .wav를 뽑아 파형을 비교하면 된다. 피치만이 아니라 파형/스펙트럼 비교가
  필요하므로 `wavnotes.py`에 별도 판독 모드가 필요할 것.

---

## 8. SFX 필터 (2026-11-17)

공개된 구현이 없어(PICO-8은 비공개, 웹 검색으로도 못 찾음) 전부 오라클 .wav export로 역산했다.
분석 요령: 효과음마다 오실레이터 시작 위상이 달라서, 샘플 비교 전에 **상관으로 위상을 맞춰야** 한다.

### 8-1. 인코딩 (byte 64) — 실측 확인

bit0 편집 모드(소리 무관), bit1 noiz, bit2 buzz, detune = (b/8)%3, reverb = (b/24)%3, dampen = (b/72)%3.

### 8-2. 실측 규칙 (22050Hz 기준)

- **reverb**: 출력을 되먹임하는 메아리 `y[n] = x[n] + 0.5·y[n-D]`, D = 366(reverb 1, 2틱) / 732(reverb 2, 4틱).
  위상 정렬 후 오차 10~18%. 소리가 끝난 뒤에도 꼬리가 울린다.
- **dampen**: 1차 저역 통과 `y += k·(x - y)`, k = 0.465(1) / 0.265(2). 삼각파 기준 오차 0.2~0.6%.
- **detune**: 절반 음량의 두 번째 목소리, 합에 5/6. `(w(φ) + 0.5·w(r·φ))·5/6` (진폭 최소제곱 A≈0.833, B≈0.417).
  비율 r은 파형마다: 삼각파 0.753 / 1.499(4도 아래 / 5도 위), 위상기 0.976 / 1.983,
  파형 1..5의 detune 2는 1.990. **파형 1..5의 detune 1은 비율이 아니다**: 맥놀이가 키 24/36/48에서
  1.36/2.40/4.37Hz → `f2 = f·0.996227 - 0.426Hz`로 맞췄다.
- **noiz** (노이즈 파형만): 무작위 값(±0.25, 균일)을 **(64 - key) 샘플** 유지하는 샘플 앤 홀드.
  키 12/24/36/48에서 52/40/28/16샘플. 가끔 값 사이를 경사로 잇는 구간이 있다(미구현).
- **buzz**: 파형마다 모양이 바뀐다(삼각파는 비대칭, 사각파 듀티 50→41%, 펄스 31.6→22% 등).
  식으로 안 떨어져서 **측정한 한 주기 표**(256점, 키 24)를 쓴다: `tools/gen_buzz.py` → `src/buzz_table.inc`.
  키·음량에 무관(키 12~48, 음량 3/7 확인). 예외: buzz 톱니파는 키 12에서 ~8% 작다(키 24 이상과 일치).
  위상기 buzz는 buzz 삼각파로 기본 위상기처럼 합성(측정과 RMS 0.381 vs 0.382).
- **커스텀 악기**: 바깥 효과음과 악기 어느 쪽 필터든 켜면 적용(OR/최대값, 두 번 걸리지 않음).
  단 악기의 reverb는 바깥 음이 끝날 때 꼬리가 잘린다(pico-c는 울리게 둠 — 차이).

### 8-3. 구현

- `src/audio.cpp`: `struct SfxFilters` + `sfx_filters()`(디코드), `channel_sample()`(채널 한 샘플:
  noiz → buzz/detune 목소리 → 음량 → dampen → reverb), `voice()`, `detune_freq()`.
  믹서(`fill_buffer`)와 `.wav` export가 같은 `channel_sample()`을 쓴다. 쉬는 채널도 reverb 꼬리를 낸다.
- 회귀: `tests/audio/carts/filters.p8` + `expected/filters.txt` 18케이스. `wavnotes.py`에 특징값
  판독(rms/harm/peak/hold)과 허용 오차 비교를 추가. 전체 **87 passed**(트랜스크립트 45 + 악기 24 + 필터 18),
  오라클 자기 비교 42/42.

### 8-4. 남은 차이

- 일반 노이즈(필터 없는 파형 6)의 스펙트럼이 원래부터 오라클과 다르다(저역이 과다, 키 48에서 고역 부족).
- noiz의 경사 구간, 노이즈의 detune/buzz, 악기 reverb 꼬리, 키 24 미만 buzz 톱니파.
- 음 타이밍 오프셋(§7)은 그대로.

---

## 9. 로드맵 일괄 진행 (2026-11-17, 브랜치 `feat/roadmap-batch`)

### 9-1. 일반 노이즈 (파형 6)

- PICO-8 노이즈는 키가 높을수록 밝고 크다(RMS 0.40 → 0.87, 키 0 → 63). 샘플은 무작위라 일대일 비교는
  불가하고 스펙트럼을 맞춘다. 원시 샘플을 보면 2샘플씩 같은 값(내부 ~11025Hz), 105 단위 양자화,
  ±0.62 부근 클램프와 간헐적 스파이크가 있다 — 이 메커니즘 자체는 재현하지 않았다.
- 모델: 키마다 `out = g·(lowpass(r; k) + m·r)` (r 균일 백색 잡음, 22050Hz). 64키 전부 오라클에서
  뽑아 맞춤(로그 스펙트럼 오차 ~0.25 = 측정 잡음 수준), 크기는 측정 RMS로 보정.
  `tools/gen_noise.py`(numpy/scipy 필요) → `src/noise_table.inc`. 다른 출력 속도에서는 필터를 늘이고
  √(22050/sr)로 보정.
- 결과: 키별 RMS가 PICO-8의 실행 간 편차(±0.02) 수준 안. 회귀 `tests/audio/carts/noise.p8`(rms, 허용 0.04).

### 9-2. 음 타이밍 = 틱 단위 계단

"1틱 늦음"의 정체는 지연이 아니라 **효과를 틱(183샘플)마다 한 번 계산해 유지**하는 것이었다.

- slide: **주파수가 아니라 피치(키)에 대해 선형**, 틱마다 계단 (261→288→311→343… 실측이 2^(t) 계열과 일치)
- vibrato: 음 높이에서 시작해 위로 가는 ±1/4 반음 삼각파, **16틱 한 주기, 2틱마다 한 계단**
  (기존 구현은 최고점에서 시작하는 매끄러운 곡선)
- arp: fast 4틱 / slow 8틱마다 한 계단(speed ≤ 8이면 절반) — 기존 `7.5 Hz` 근사 대신 틱 정수로
- drop, fade in/out: 음의 틱에 대해 선형 → fade in의 첫 틱은 무음 (오라클에서 보이던 현상)
- 구현: `note_effect()`가 `tick = floor(offset·speed)` 기준으로 계산. 커스텀 악기의 바깥/안쪽 효과도 같이 적용됨.
- 남은 차이: PICO-8 export의 시작 지연(34~215샘플, 실행마다 다름)은 재현 불가 → 짧은 창의 slide 판독이
  흔들려서 회귀(`tests/audio/carts/timing.p8`)는 arp(4틱 창)와 fade in만 검사.

### 9-3. 성능 + z8lua 패치

- 벤치: `pico-c -headless -frames N <cart>` → 프레임당 평균/최악 ms를 stderr로. 무거운 합성 프레임
  (스프라이트 300, circfill 150, pset 3000, tline 64줄, sin/cos/sqrt 4000회, 테이블 1000개)이 ~2-3ms
  (60fps 예산 16.7ms). 로컬 test_carts는 타이틀 화면이라 1ms 안팎.
- `sample` 프로파일 핫스팟 두 개를 z8lua에서 고침:
  - `sqrt`: 24회 abacus 루프 → FPU sqrt + 보정. 5240만 값에서 **비트 단위 동일** 확인. 샘플 486 → 76.
  - 숫자처럼 생긴 문자열 키 변환(`luaH_newkey`/`luaH_get`)이 **모든** 새 문자열 키마다 `strtod`를
    부르고 있었음 → 첫 글자로 숫자일 수 없는 키를 걸러냄. 프로파일에서 사라짐.
- **발견**: 로컬 z8lua 체크아웃에 커밋되지 않은 수정(파서 수정, 숫자 문자열 키)이 있었고 저장소에는
  없었다 → 새로 클론하면 빠지는 상태였다. 이제 `platforms/common/z8lua.patch`로 관리하고 Makefile이
  z8lua를 리비전 `8575dfbc`에 고정한 뒤 패치를 적용한다(이미 적용돼 있으면 건너뜀, 다른 변경이 있으면
  오류). 새 클론에 패치가 적용되는 것까지 확인.

### 9-4. 작은 항목

- **문법 에러 블록** (24케이스 오라클 대조, 전부 일치):
  - 파서 오류 줄 = **마지막으로 읽은 토큰의 줄**(`luaX_syntaxerror`에서 `lastline`, z8lua 패치) —
    줄 맨 앞의 stray `end`는 앞 줄의 오류
  - 닫히지 않은 키워드: `syntax error` / `unclosed if at line N (tab 0)` / 빈 줄
  - 닫히지 않은 괄호: `syntax error line N (tab 0)` / N번 줄 / `unclosed (` (파일 끝이면 마지막 줄 없음)
  - 자기 줄을 말하는 오류(goto 라벨): `syntax error` / 메시지 + ` (tab 0)`
  - 회귀: `cmp_syntax_block/paren/goto/line.p8`
- **extcmd 캡처**: `screen`(384x384, PICO-8과 픽셀 동일), `audio_rec`/`audio_end`(22050Hz wav, 헤드리스에선
  빈 파일 — PICO-8 -x와 같음), `set_filename`(정확한 이름, 덮어씀; 없으면 `<카트>_N` 순번). 위치는
  `$PICO_C_DESKTOP` 또는 ~/Desktop. `label`, gif는 미구현.
- **serial**: 0x805 = RAM → stdout(그대로), 0x804 = stdin → RAM(터미널이면 읽지 않음), 옮긴 바이트 수 반환.
  회귀 `cmp_serial.p8`.
- **import/export png**: `export foo.png` = 스프라이트 시트 128x128 (PICO-8과 픽셀 동일), `import foo.png` =
  가장 가까운 팔레트 색으로 시트에 읽기(매핑은 오라클 미검증 — 실행 중 카트의 `import()`는 PICO-8에서
  RAM을 바꾸지 않아 비교 불가).
- 덤: `serial`/`add`의 잘못된 doc 주석 수정, API_CHECKLIST의 `serial`/`menuitem` 중복 정리.

### 9-5. 결과

`test.sh` **99 passed**(트랜스크립트 50 + 오디오 49), `oracle.sh` 48 MATCH + 2 KNOWN, `oracle_audio.sh` 전부 일치.

### 9-6. 남은 것

- 셸: `keyconfig`(UI 필요), `splore`/BBS(온라인), html/bin export.
- 노이즈 메커니즘(11025Hz 쌍, 양자화, 클램프) 자체의 재현, PICO-8 export 시작 지연.
- (§10에서 처리: 라벨, gif, 컴파일 경고)

---

## 10. CI, 실제 게임 호환성, 캡처, 경고 (2026-11-17, 브랜치 `feat/ci-compat-capture`)

### 10-1. CI

- `.github/workflows/test.yml`: Linux(apt `libsdl2-dev`)와 macOS(Homebrew `sdl2`, `SDL2_DIR`)에서
  새 체크아웃 → `build.sh` → `test.sh`. z8lua 클론 + 패치 적용까지 검증된다.
- `build_common.sh`의 macOS 분기가 `SDL2_DIR`을 받도록(기본 MacPorts `/opt/local`).
- 이 기기에는 Docker가 없어 **CI는 브랜치를 푸시해야 처음 실행된다**(로컬 검증 불가).

### 10-2. 실제 게임 카트 (몽키 테스트)

- `pico-c -headless -frames N -autoplay SEED cart`: 시드로 정해지는 무작위 버튼 입력(4..34프레임마다
  새 조합, 대부분 O/X 포함)으로 게임을 타이틀 너머까지 진행. 벤치 줄에 `stopped by an error` 표시.
- `test_carts/`의 게임 7종을 5분(게임 시간)씩 시드 3~5개로 돌려 찾은 **pico-c 버그** (전부 오라클 실측으로 수정):
  - **tinyhawk** `line(x,y)` 2인자: 마지막 끝점(0x5f3c/0x5f3e)에서 이어 그림, `line(x,y,c)`도 같음,
    `line()`은 끝점을 잊음(0x5f35=1 → 다음 호출은 시작점만 설정). 부팅 시 끝점 (0,0) 유효.
  - **delunky** `del(nil, x)`: PICO-8은 오류 없이 값 0개. `add/count/deli`도 nil이면 값 0개.
  - **celeste** `foreach` 중 원소 삭제: PICO-8의 순회는 "현재 칸이 직전 값과 같을 때만 전진, 구멍 건너뜀"
    → 현재 원소를 지워도 다음 원소를 건너뛰지 않음, 뒤에 추가한 원소도 방문.
  - 부수 발견: `del`은 **첫 번째** 일치만 지우고 그 값을 반환(기존: 모두 지움), `add`는 값을 반환하고
    `add(t,v,i)` 삽입 지원(범위 밖은 `position out of bounds`), `count(t,v)`는 문자열/소수도 비교.
  - 색 인자를 받은 모든 도형이 그 색을 드로우 색(0x5f25, 바이트 전체)으로 남김.
- 게임 쪽 문제로 판정: **picodriller** 시드 4 — 크레딧 메뉴를 여는 같은 프레임에 위를 누르면 설명 없는
  항목으로 이동(카트 코드 버그, PICO-8에서도 같음. 메뉴 파싱 결과가 PICO-8과 동일함을 확인).
  **celeste_v2**는 코드 영역이 `load fail` 텍스트뿐인 깨진 파일, **solitomb**은 없는 카트(`solitomb_title`)를 로드.
- 수정 후 6종 × 시드 3~5개, 각 5분 동안 오류 0.

### 10-3. 숨은 팔레트 (128..143)

- 렌더러가 화면 팔레트의 상위 비트를 버려서 `pal(c, 128+n, 1)`이 기본 색으로 보였다.
- 실측 규칙: 표시 색 = `값 & 0x8f`(7번 비트 = 숨은 색, 4..6번 비트 무시). `pal(...,1)`과 `pal(...,2)`는
  바이트 전체 저장, 세컨더리 팔레트는 RAM 0x5f60(기존: 별도 버퍼)에 있음.
- `pico8_palette_ext` + `pico8_display_rgb()`(pico8_core.h). 스크린샷이 PICO-8과 픽셀 동일.

### 10-4. 라벨과 gif

- `extcmd("label")`: 화면(화면 팔레트 거침, 숨은 색은 g..v)을 라벨로. `__label__`(128×128자, `__gff__` 뒤
  `__map__` 앞)로 저장: `save()`/쓰기 경로는 `cart_write_p8`, 셸 `save`(원문 유지)는
  `pico8_cart_splice_label()`로 섹션을 교체/삽입. `.p8` 로드 시 라벨 보존. `.p8.png` 라벨 그림은 미구현.
  PICO-8은 실행 중 카트의 `save()`를 무시해 파일 형식을 오라클로 확인할 수 없었다(문서 형식을 따름).
- `extcmd("rec")` / `extcmd("video")`: `src/video.cpp`. 카트가 그린 프레임을 32색 번호로 16초 링 버퍼에
  보관, `video`는 `rec` 이후(없으면 최근 8초)를 3배 크기 gif(32색 전역 색표, LZW)로. 프레임 간격은
  30fps면 3/4cs 교대. 독립 디코더로 풀어 같은 프레임 스크린샷과 픽셀 동일 확인.

### 10-5. 경고 정리 → 메모리 버그 2개

- 경고 9개 제거. `api_memory.cpp`의 "항상 거짓" 비교를 고치며 실측:
  - **주소는 64KB에서 되돌아감**: `poke(0xfffe,1,2,3,4)`는 0,1번지에 이어 씀, `poke2(0xffff)`의 상위 바이트는 0번지,
    `memset`/`peek4`/`memcpy`도 되돌아감(기존: 중간에 멈추거나 nil).
  - **`peek4`/`poke4`가 소수부를 잃고 있었음**: 32비트를 정수로 변환하던 것을 16.16 원시 비트로
    (`poke4(a,0x1234.5678)` → `peek4` 동일, 바이트 1,2,3,4 → `0x0403.0201`).

### 10-6. 결과

`test.sh` **103 passed**(트랜스크립트 54 + 오디오 49), `oracle.sh` 52 MATCH + 2 KNOWN, 경고 0.
새 회귀 카트: `cmp_tables`, `cmp_line_pen`, `cmp_palette`, `cmp_memwrap`.

---

## 11. 잠재 버그 수정 (2026-11-17)

- **셸의 카트 이름/소스가 파일마다 따로 있었음**: `shell_internal.h`가 `g_cart_name`/`g_source`를
  `static`으로 정의해서 host.cpp(불러올 때 씀), console.cpp, commands.cpp(읽음)가 각자 사본을 가졌다.
  그래서 명령들은 늘 빈 값을 봤다: `backup()`은 "no editable source"로 조용히 실패, 이름 없는 `save`는
  빈 이름으로, `info`/`edit`도 영향. → 헤더는 `extern`, 정의는 console.cpp 하나.
- **카트 정체를 실행 뒤에 정하고 있었음**: `pico8_console_load`가 소스를 카트 최상위 코드 실행 *후*에
  읽어서, 최상위에서 부른 셸 명령(`backup()` 등)이 소스를 못 봤다. → 경로·이름·텍스트·`g_has_cart`를
  실행 전에 정하고 실패하면 되돌림. 짧게 읽힌 파일의 크기 처리 버그(`resize(sz)` → 읽은 만큼)도 수정.
- **실행 중 `import()`가 카트 ROM을 안 바꿨음**: PICO-8은 상대 경로의 그림을 RAM과 카트 ROM에 모두
  넣는다(`reload()` 뒤에도 남음). 앞서 "PICO-8은 RAM을 안 바꾼다"고 본 것은 절대 경로를 PICO-8이
  찾지 못했기 때문이었다. 같은 그라데이션에서 체크섬(-21079)이 PICO-8과 같아 **색 매칭도 일치** 확인.
  회귀 `cmp_import.p8` + `import_grad.png`(오라클은 자기 카트 폴더에서 찾으므로 KNOWN, 기대값은 실측).
- 결과: `test.sh` **104 passed**, `oracle.sh` 52 MATCH + 3 KNOWN.

---

## 12. Windows 네이티브 빌드 (MSVC) (2026-11-17, 브랜치 `feat/msvc`)

- **이식성**: POSIX 호출을 `src/util.{h,cpp}`의 헬퍼로 모았다 — 파일 시스템은 `std::filesystem`
  (`util_list_dir`, `util_is_dir/file`, `util_cwd`, `util_chdir`, `util_make_dir`), 나머지는 시스템별 대응
  (`util_popen/pclose` = `_popen`, `util_stdin_is_terminal` = `_isatty`, `util_local_time/utc_time` =
  `localtime_s/gmtime_s`). GNU 복합 리터럴 1곳 제거. `system("mkdir -p backup")`도 헬퍼로(Windows cmd에서는
  `-p` 폴더가 생겼을 것).
- **CMake**: `CMakeLists.txt` — z8lua 클론/고정/패치를 Makefile과 같게(configure 단계), z8lua `.c`를 C++로,
  SDL2는 `find_package(SDL2 CONFIG)`(vcpkg/공식 VC 패키지/MacPorts), `<SDL2/SDL.h>`의 상위 폴더 탐색,
  MSVC는 `/utf-8 /W3`, `_CRT_SECURE_NO_WARNINGS`, SDL2.dll을 실행 파일 옆에 복사, macOS는 ad-hoc 서명.
  macOS에서 CMake 빌드로도 104/104 통과 확인.
- **테스트 실행기**: `tests/tools/run_tests.py` — bash 스크립트의 트랜스크립트/오디오 비교를 Python으로 옮긴
  유일한 구현. `\r\n` 정규화, 마스크(sed 치환식) 지원, `audio-check`/`audio-record` 하위 명령.
  `test_common.sh`와 `audio_common.sh`는 이것을 부르는 껍데기. 결과 104/104 동일, 실패 검출 확인.
- **줄바꿈**: `.gitattributes`로 텍스트는 항상 LF(.bat만 CRLF). Windows Git의 CRLF 변환이 바이트 크기
  테스트와 z8lua 패치를 깨기 때문. z8lua 클론은 `-c core.autocrlf=false`.
- **스크립트/CI**: `platforms/windows/build-msvc.bat`, `test-msvc.bat`, CI `windows-msvc` 작업
  (vcpkg `sdl2:x64-windows` → CMake → `run_tests.py`).
