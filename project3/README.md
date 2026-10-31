# Project 3 — 가상 메모리와 페이지 폴트

[전체 프로젝트 소개](../README.md)

## 기반 코드

Project 2를 이어서 가상 메모리 기능을 추가했습니다. nice 시스템 콜과 CFS 스케줄러를 유지하고, 타이머 인터럽트의 time slice 차감을 `slice -= 1000`으로 수정했습니다.

## 목표

가상 주소 영역의 등록과 물리 페이지 할당을 분리하고, 첫 접근의 페이지 폴트를 처리하는 메모리 매핑 기능을 구현했습니다. 파일 기반 매핑과 익명 매핑, 즉시 할당과 지연 할당의 차이를 다뤘습니다.

## 구현 내용

| API | 역할 |
| --- | --- |
| `uint mmap(uint addr, int length, int prot, int flags, int fd, int offset)` | 매핑 영역 등록과 주소 반환. 실패 시 `0` |
| `int munmap(uint addr)` | 시작 주소에 해당하는 매핑과 할당된 페이지 해제. 성공 `1`, 실패 `-1` |
| `int freemem(void)` | 남은 물리 페이지 수 조회 |

`MMAPBASE`인 `0x40000000`을 기준으로 주소를 구성하고, 전역 `mmap_areas[64]`에 프로세스별 매핑 메타데이터를 저장합니다. `PROT_READ`·`PROT_WRITE`로 권한을 지정하며 `MAP_ANONYMOUS`는 익명 매핑, `MAP_POPULATE`는 즉시 물리 페이지 할당에 사용합니다.

지연 할당 영역에 접근하면 `trap.c`에서 폴트 주소와 쓰기 여부를 전달합니다. 핸들러는 매핑과 권한을 확인한 뒤 페이지를 할당하고 익명 페이지를 초기화하거나 파일 내용을 읽습니다. `fork()`에는 매핑 정보와 이미 할당된 페이지를 자식에게 복사하는 처리를 추가했습니다.

## 주요 코드

- [proc.c](proc.c), [proc.h](proc.h): 매핑 자료구조, `mmap`, `munmap`, 페이지 폴트 처리, fork 시 복사
- [trap.c](trap.c): 사용자 페이지 폴트 전달과 실패 처리
- [kalloc.c](kalloc.c): 할당·해제 시 free page 개수 관리
- [param.h](param.h), [memlayout.h](memlayout.h): 매핑 플래그와 주소 기준
- [sysproc.c](sysproc.c), [syscall.c](syscall.c), [syscall.h](syscall.h), [usys.S](usys.S), [user.h](user.h): 시스템 콜 연결
- [mytest.c](mytest.c): 익명 매핑, 파일 매핑, fork, unmap 확인

## 실행과 확인

빌드에 필요한 도구는 [루트 README](../README.md#빌드와-실행)를 참고합니다.

```sh
make clean
make CPUS=1 QEMU=qemu-system-i386 qemu-nox
```

xv6 셸에서 `mytest`를 실행합니다. 테스트는 익명 영역에 `HOON`을 기록하고, 원본 `README` 파일을 즉시 매핑해 읽으며, fork 이후의 데이터와 매핑 해제 전후 free page 수를 출력합니다.

수업에서 정한 인터페이스에 맞춰 구현했습니다. 파일 매핑의 변경 내용을 디스크에 반영하는 write-back은 다루지 않습니다.
