# 건물 부시기 (斬るビル) — Windows 11 64비트 포팅

2003년 일본 졸업연구 작품 **「斬るビル」(卒研2003)** 의 한국어판(`시작.exe`, 건물 부시기)을
디컴파일·리버스 엔지니어링해 C++로 다시 구현한 소스 포트입니다. 게임 로직은 원본 기계어를
함수 단위로 1:1 번역했고, Windows 11에서 문제가 되는 부분(DirectDraw 8비트 전체화면, DirectMusic 등)만
현대적인 구현으로 교체했습니다.

* 원본과 **완전히 같은 동작**: 원본에 들어 있던 리플레이 20개(1막 10개, 2막 10개)를 재생하면
  최종 점수와 최대 콤보가 원본 랭킹 파일(`score.sav`, `daini_score.sav`)과 20개 모두 일치합니다.
  난수, 정수 오버플로, x87 부동소수점 결과까지 비트 단위로 같아야 나오는 결과입니다.
* **화면도 픽셀 단위로 동일**: Wine에서 돌린 원본과 비교했을 때 타이틀, 랭킹 화면, 게임 플레이 화면이
  0픽셀 차이로 일치합니다.
* 외부 런타임 불필요: `KiruBill.exe` 하나(x64, 정적 링크)에 Windows 기본 DLL만 사용합니다.

## 실행 방법 (Windows 10/11 64비트)

이 저장소의 [`game/`](game) 폴더에 원본 게임 데이터와 미리 빌드한 실행 파일이 함께 들어 있습니다.

1. 저장소를 내려받습니다 (GitHub의 **Code → Download ZIP** 또는 `git clone`).
2. `game\KiruBill.exe`를 실행합니다.

다른 곳에 있는 원본 게임 폴더(`시작.exe`와 `image/`, `sound/`, `data/`, `save/`가 있는 곳)에
`KiruBill.exe`만 복사해서 실행해도 됩니다. 세이브·랭킹·리플레이 파일은 원본과 같은 형식이라 그대로 이어서 쓸 수 있습니다.

> 게임 폴더는 쓰기 가능한 위치(예: 문서, 바탕화면)에 두세요. `C:\Program Files` 아래에서는
> 세이브 파일을 쓸 수 없습니다 (원본과 동일).

### 조작

| 키 | 동작 |
|---|---|
| ← → | 레인 이동 / 메뉴에서 모드(第一幕·第二幕·とことん) 전환 |
| ↑ | 점프 / 메뉴 커서 |
| ↓ | 방어 / 메뉴 커서 |
| Z | 베기, 메뉴 결정 |
| X | 필살기 (게이지 300 이상) |
| Esc | 종료 |
| Tab | 리플레이 빨리 감기 (누르고 있는 동안) |
| Z 2초 이상 누른 뒤 떼기 | 리플레이 건너뛰기 |
| Alt + Enter | 전체화면 ↔ 창 모드 (포트에서 추가) |

게임패드도 원본처럼 지원합니다(Windows `게임 컨트롤러 설정`(joy.cpl)에 보이는 패드):
방향 = 이동, 버튼1 = 베기/결정, 버튼2 = 필살기, 버튼3 = 방어. 원본과 달리 게임 도중 연결해도 인식합니다.

### 실행 옵션

| 옵션 | 설명 |
|---|---|
| `-window` | 창 모드로 시작 (기본은 테두리 없는 전체화면, 4:3 비율 유지) |
| `--gdi` | OpenGL 대신 GDI로 화면 출력 (그래픽 드라이버 문제 시) |

## 원본에서 바뀐 점 (Windows 11 대응)

| 원본 | 포트 |
|---|---|
| DirectDraw 640×480×8bit 독점 전체화면, 해상도 변경 | 8비트 화면을 소프트웨어로 똑같이 합성한 뒤 32비트로 변환해 OpenGL(실패 시 GDI)로 출력. 해상도 변경 없음, 4:3 레터박스 |
| DirectMusic (MIDI, 루프 포인트) | 자체 MIDI 시퀀서 + Windows 기본 MIDI 신스(Microsoft GS Wavetable Synth, DirectMusic과 같은 Roland GS 음색). 루프 구간(DirectMusic MUSIC_TIME)과 루프 경계 음표 처리까지 재현 |
| DirectSound | 그대로 DirectSound (원본과 같은 버퍼 플래그·재생 방식) |
| GetKeyboardState / joyGetPosEx | 그대로 사용 (조이스틱 핫플러그 추가) |
| `HGS行書体` 글꼴 (Office 전용) | 같은 크기(28×14)로 `HGS行書体` → `ＭＳ 明朝` → `游明朝` → … 순서로 있는 글꼴 사용 |
| 마우스 커서 숨김 | 게임 화면 위에서만 숨김 |

## 빌드

### Windows (Visual Studio 2022)

```bat
cmake -B build -A x64
cmake --build build --config Release
```

### Linux에서 Windows용 교차 컴파일 (MinGW-w64)

```bash
sudo apt install g++-mingw-w64-x86-64 cmake
cmake -B build-win64 -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build-win64 -j
```

### Linux / macOS (SDL2, 개발·테스트용)

```bash
sudo apt install libsdl2-dev libfreetype-dev cmake
cmake -B build && cmake --build build -j
```

SDL2 빌드는 테스트용이라 MIDI 배경음악은 나오지 않습니다.

## 검증

```bash
python3 tests/run_replays.py build/KiruBill game
```

원본 게임 폴더의 리플레이 20개를 헤드리스 모드로 재생해 원본 랭킹 파일과 점수·콤보를 비교합니다.
결과(리눅스 빌드, MinGW Windows 빌드 모두):

```
replay 1: score=17048529 combo=148  expected score=17048529 combo=148  OK
...
replay20: score=19114230 combo=150  expected score=19114230 combo=150  OK
20/20 replays reproduce the original results
```

헤드리스 테스트 옵션(`--headless --frames N --trace --input 파일 --shots 폴더 --find 기준.bmp`)은
`src/engine/testing.h`에 설명되어 있습니다.

## 저장소 구조

```
src/game/part_a..f.cpp   원본 게임 함수 1:1 번역 (함수 이름 = 원본 주소, 예: f_401040)
src/game/mem.h           원본 전역 변수를 원래 주소 그대로 접근 (MI32(0x41f6dc) 등)
src/engine/              DirectX 래퍼·태스크 스케줄러 대체 구현 (렌더러, 사운드, MIDI, 입력)
src/platform/win32.cpp   Windows 백엔드 (기본)
src/platform/sdl.cpp     SDL2 백엔드 (Linux/macOS 개발용)
docs/                    리버스 엔지니어링 문서, 번역 규칙, 함수별 분석 노트
tools/decompile/         디컴파일에 사용한 스크립트 (angr)
tests/run_replays.py     리플레이 회귀 테스트
game/                    원본 게임 데이터 (image, sound, data, save) + 미리 빌드한 KiruBill.exe
```

자세한 분석 내용은 [docs/REVERSE_ENGINEERING.md](docs/REVERSE_ENGINEERING.md)를 보세요.

`game/` 폴더의 이미지·사운드·데이터는 원작(卒研2003 「斬るビル」)과 한국어판 배포본의 파일 그대로이며,
저작권은 원작자에게 있습니다. 원본 실행 파일(`시작.exe`)은 포함하지 않았습니다.
