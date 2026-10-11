# 통신공학 (Communication Engineering)

통신공학 과목의 실습 및 과제를 위한 프로젝트입니다.

## 프로젝트 구조

```text
.
├── Communication-Engineering.slnx
├── Homework-01
│   ├── Homework-01.vcxproj
│   ├── Homework-01.vcxproj.filters
│   ├── main.c
│   └── sample.wav
├── LICENSE.txt
├── Makefile
└── out/
```

## 개발 환경

### Linux

- GCC
- GNU Make

### Windows

- Visual Studio 2022 이상
- Desktop Development with C++ 워크로드

## 빌드 및 실행

### Linux

프로젝트를 빌드하고 실행하려면 다음 명령어를 사용합니다.

```bash
make run
```

빌드만 수행하려면:

```bash
make
```

빌드 결과물을 삭제하려면:

```bash
make clean
```

실행 파일 및 오브젝트 파일은 `out/` 디렉터리에 생성됩니다.

### Visual Studio

1. `Communication-Engineering.slnx` 열기
2. 원하는 빌드 구성(Debug/Release) 선택
3. **디버깅하지 않고 시작(Ctrl + F5)** 실행

Visual Studio 환경에서는 `sample.wav` 경로가 코드에 포함되어 있으므로 별도의 명령행 인자를 입력할 필요가 없습니다.

## 입력 파일

기본 입력 파일은 다음과 같습니다.

```text
Homework-01/sample.wav
```

Linux의 경우 `make run` 명령이 자동으로 해당 파일을 프로그램에 전달합니다.
