# Project 2 — CFS CPU 스케줄링

[전체 프로젝트 소개](../README.md)

## 기반 코드

Project 1에서 만든 시스템 콜과 nice 값을 유지하고, 그 위에 CFS 스케줄러를 구현했습니다.

## 목표

Project 1의 nice 값을 이용해 xv6의 기본 Round Robin 스케줄러를 CFS 개념에 따른 스케줄러로 변경하는 과제입니다. nice에 따른 가중치, 가상 실행 시간(vruntime), time slice로 CPU 사용량을 분배하도록 구현을 시도했습니다.

## 구현 내용

- nice 0~39에 대응하는 고정 weight 테이블을 사용합니다.
- RUNNABLE 프로세스를 순회해 vruntime이 가장 작은 프로세스를 선택하고 전체 가중치를 구합니다.
- `10 ticks × 선택 프로세스 weight / 전체 weight`를 바탕으로 time slice를 계산합니다.
- 타이머 인터럽트에서 runtime, vruntime, 남은 slice를 갱신하고 slice 소진 시 `yield()`합니다.
- `fork()` 시 부모의 nice·runtime·vruntime을 복사하고, 깨어나는 프로세스의 vruntime을 조정합니다.
- `ps()`에 runtime/weight, runtime, vruntime, 전체 tick 출력을 추가했습니다.

프로세스 테이블을 선형 탐색하는 수업용 구현입니다. Linux CFS의 자료구조와 모든 동작을 재현한 구현은 아닙니다.

## 주요 코드

| 파일 | 확인할 내용 |
| --- | --- |
| [proc.c](proc.c) | weight 테이블, `scheduler`, `wakeup1`, `fork`, `ps` |
| [proc.h](proc.h) | runtime, vruntime, slice 필드 |
| [trap.c](trap.c) | 타이머 인터럽트의 시간 갱신과 선점 |
| [mytest.c](mytest.c) | fork와 nice 변경을 포함한 연산 부하 테스트 |

## 한계

**time slice 계산과 차감의 단위가 맞지 않습니다.** `scheduler()`는 `1000 * 10 * weight / total_weights`로 millitick 단위 slice를 계산하지만, `trap.c`는 매 tick마다 `slice--`로 1만 차감합니다. 이 때문에 의도한 것보다 긴 time slice가 할당됩니다. 이 부분은 Project 3에서 `slice -= 1000`으로 수정했습니다.

당시 25/100을 받은 과제입니다. 이 폴더에는 제출 당시 코드를 유지했습니다.

## 실행과 확인

빌드에 필요한 도구는 [루트 README](../README.md#빌드와-실행)를 참고합니다.

```sh
make clean
make CPUS=1 QEMU=qemu-system-i386 qemu-nox
```

xv6 셸에서 `mytest`를 실행합니다. fork와 nice 변경 전후의 runtime·vruntime을 출력하는 테스트입니다. 모든 nice 조합의 CPU 점유율이나 wake-up 규칙을 검사하지는 않습니다.
