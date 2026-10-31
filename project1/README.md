# Project 1 — 시스템 콜

[전체 프로젝트 소개](../README.md)

## 기반 코드

기본 xv6 코드에 프로세스 정보 조회와 nice 값 변경을 위한 시스템 콜을 추가했습니다.

## 목표

사용자 프로그램이 프로세스의 nice 값을 조회·변경하고 프로세스 정보를 출력하도록 `getnice`, `setnice`, `ps` 시스템 콜을 추가했습니다. 사용자 함수 호출이 트랩과 시스템 콜 디스패치를 거쳐 커널 함수로 연결되는 흐름을 다뤘습니다.

## 구현

| API | 동작 |
| --- | --- |
| `int getnice(int pid)` | 대상 프로세스의 nice 값 반환. 유효하지 않은 PID는 `-1` |
| `int setnice(int pid, int value)` | nice 값을 0~39 범위로 변경. 성공 `0`, 실패 `-1` |
| `void ps(int pid)` | 이름, PID, 상태, nice 출력. PID가 0이면 전체 대상 출력 |

`proc` 구조체에 nice 필드를 추가하고 프로세스 할당 시 기본값을 20으로 설정했습니다. 프로세스 테이블을 탐색하는 동안 `ptable.lock`을 사용합니다. 이 과제의 범위는 nice 값의 저장·조회이며 스케줄링 정책 변경은 Project 2에서 다룹니다.

## 주요 코드

- [proc.c](proc.c), [proc.h](proc.h): nice 저장과 `getnice`, `setnice`, `ps` 구현
- [usys.S](usys.S), [syscall.h](syscall.h), [syscall.c](syscall.c): 사용자 진입점, 시스템 콜 번호와 디스패치 등록
- [sysproc.c](sysproc.c): 인자 추출 및 커널 함수 호출
- [user.h](user.h), [defs.h](defs.h): 사용자·커널 선언
- [mytest.c](mytest.c): 시스템 콜 확인용 사용자 프로그램

## 실행과 확인

빌드에 필요한 도구는 [루트 README](../README.md#빌드와-실행)를 참고합니다.

```sh
make clean
make CPUS=1 QEMU=qemu-system-i386 qemu-nox
```

xv6 셸에서 `mytest`를 실행해 nice 값 조회·변경, 잘못된 입력의 `-1` 반환, 프로세스 정보 출력을 확인합니다.
