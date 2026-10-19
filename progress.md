# pico-c 진행 상황 & TODO

> 새 세션에서 이어서 진행하기 위한 문서입니다. 가장 최근 갱신: 2026-10-19 (6단계 완료)

---

## 0. 환경 / 규약 (중요)

- **작업 디렉터리**: `/Users/mumal/Documents/pico-c` (이미 git 저장소)
- **빌드**: `make` → `pico-c.exe`
- **코드사인 필수**: 빌드마다 `codesign --force --sign - pico-c.exe`
  (amfid가 깨져있어 안 하면 실행 불가)
- **오라클(실제 PICO-8) 사용법**: `/Applications/PICO-8.app`는 멈춤 → 반드시 사본 사용
  ```
  ditto /Applications/PICO-8.app /tmp/p8app
  codesign --force --deep --sign - /tmp/p8app
  ```
  실행은 백그라운드 + 출력 리다이렉트, 6~7초 대기 후 강제 종료:
  ```
  ( /tmp/p8app/Contents/MacOS/pico8 -x "$PWD/<cart>.p8" > /tmp/a_out.txt 2>&1 & )
  sleep 6; pkill -f p8app
  grep -v '^RUNNING' /tmp/a_out.txt
  ```
- **비교 규칙**: `diff <(grep -v '^DEBUG' ours.txt | sed '/^$/d') <(grep -v '^RUNNING' ref.txt | sed '/^$/d')`
- 오라클의 작업 디렉터리는 `$HOME/Library/Application Support/pico-8/carts/`이므로
  상대 경로 파일 접근은 이쪽에 생기는 걸 주의할 것.
- `z8lua` = `samhocevar/z8lua` **pico8 브랜치** (`make`가 git clone). `lua_Number` = `z8::fix32`.
  변환 헬퍼는 `src/pico8.h`: `fix32_from_int(int32_t)`, `fix32_to_int(fix32)` 등.
  `fix32(double)`, `fix32::frombits(int32_t)`도 있음.

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
- tracked 50 files, `.git` 452K. `.gitignore`로 `z8lua/`, `SDL2_mgw/`, `picotestdriver/`,
  `test_carts/`, `obj*/`, `build/`, `macbuild/`, `*.exe`, `test_*.p8`, `*.p8d` 제외
- **fresh clone 시**: macOS는 MacPorts `/opt/local`의 SDL2 필요 + `make`가 z8lua 클론(네트워크)

### 핵심 결과물
- `src/` 11개 파일: `api.cpp`, `audio.cpp`, `cart.cpp`, `gfx.cpp`, `input.cpp`,
  `main.cpp`, `memory.cpp`, `pico8.h`, `shell.cpp`, `vm.cpp`, `stb_image.h`
- **회귀 스위트 18/18 오라클 일치** (`cmp_*.p8` + `cmp.sh`)
  (`cmp_savegen`/`cmp_savereload`는 의도적 차이: PICO-8이 `-x`에서 `save()` 비활성화)
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

### 3-1. 글리프 캐릭터셋 표현 패리티 (우선순위 높음, 상세는 §5)

PICO-8은 글리프를 **단일 charset 바이트(128..153)**로 저장하는데 우리는
UTF-8 원본을 유지해서 생기는 차이:
- `#"⬅️"` → PICO-8: 1, 우리: 6 / `ord("⬅️",1)` → 139 vs 226
- `cmp_globlist`의 글리프 **정렬 순서** 차이 (이름 집합은 일치 — 유일한 남은 테스트 차이)
- 카트가 `\226\172...` 같은 raw 바이트를 조합해 출력하면 우리가 깨진 UTF-8을 씁니다

구현 방향 (작업량 큼 → z8lua가 clone되는 외부 저장소라 Makefile 패치 단계 필요):
1. z8lua 렉서 패치: 식별자 + 문자열 리터럴에서 PICO-8 글리프 코드포인트를
   charset 바이트로 변환 (`\ddd` 이스케이프는 변환하지 않음 — 오라클 실측)
2. 출력부(printh/콘솔/에러 메시지)에서 charset 바이트 → UTF-8 재변환
3. 추가 조사 필요: 128..153 말고 다른 글리프(▶ ⇧ ⌫ 등)도 변환되는지,
   `chr(139)`/`print()`/글자 그리기가 charset 코드를 어떻게 다루는지

### 3-2. 작은 것들
- [x] `src/cart.cpp`: 카트 헤더 버전 → **항상 43** (`kCartVersion`).
      근거: 오라클(0.2.7)은 불러온 카트의 버전(41)을 무시하고 cstore() 결과에
      항상 자기 버전 43을 씀. 또 우리 writer의 출력은 오라클 출력과
      **버전 줄 말고 바이트 단위로 동일** (`cmp_cstore` 결과를 직접 diff로 확인).
      그래서 `cart_parse_version()`/`pico8_cart_get_version()`은 제거.
      부수 효과: `cmp_cstore` 실행해도 `cstore_base.p8`이 더는 git diff에 안 남음.
      (참고: `save()`는 `-x` 실행에서 PICO-8이 조용히 무시함 — 파일이 안 생김)

### 3-3. 회귀 확인 방법
- `./cmp.sh <cart.p8>` 한 개, `./cmp_all.sh <cart...>` 여러 개
  (둘 다 `/tmp/p8app` 사본을 오라클로 사용, `LC_ALL=C`로 바이트 비교)
- `cmp_stat`은 `stat(84)`/`stat(94)`가 초 경계에서 1초 차이 나는 **플레이크**
  → 재실행 시 통과
- `cmp_glyphbytes.p8` / `cmp_glyphord.p8`은 §5 조사용 카트 — 캐릭터셋
  패리티가 끝나기 전까지는 DIFF가 정상

### 3-4. 이후 남은 로드맵
- [ ] **토큰 카운트**: `API_CHECKLIST.md`의 `info()`가 token/compressed 수를 못 냄
      (PICO-8 토크나이저 필요 — z8lua 파서 활용 고려)
- [ ] **성능**: 픽스포인트 연산 최적화, 병목 프로파일
- [ ] 5단계 마무리: 셸/에디터 명령 중 미구현 `edit`(내장 에디터), `splore`(온라인),
      `keyconfig`(버튼 리맵) — 현재 "not available" 안내 출력 상태
- [ ] (선택) `.p8.png` **저장** — PXA 압축기 필요 (읽기만 구현됨, 저장 미지원)
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
