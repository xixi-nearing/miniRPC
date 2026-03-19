# MiniRPC Performance Report

- Test date: 2026-03-19
- CPU: Intel(R) Xeon(R) Platinum 8336C CPU @ 2.30GHz
- Logical cores: 2
- Compiler: g++ (Ubuntu 13.3.0-6ubuntu2~24.04) 13.3.0
- Kernel: Linux 6.8.0-55-generic #57-Ubuntu SMP PREEMPT_DYNAMIC Wed Feb 12 23:42:21 UTC 2025 x86_64 GNU/Linux
- Binary: `build/benchmark_rpc`
- Raw data: `benchmarks/results/latest.csv`

## Headline Findings

- The unary `Add` benchmark reaches `19892.11` QPS at concurrency 8, with `p99=951.86 us`.
- For a 4 KB payload, `Echo` still keeps `14094.19` QPS and `p99=768.72 us`.
- Under overload, the server returns `BUSY` for `37.5%` of requests instead of hanging; successful calls stay near the configured service time (`avg=97.52 ms`).
- The direct in-process baseline shows the wire/protocol overhead is mainly in socket IO and thread scheduling rather than business logic.

## Scenario Table

| Scenario | Concurrency | Payload | Total Ops | Success | Busy | Timeout | QPS | Avg (us) | P50 (us) | P95 (us) | P99 (us) |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| local_add_baseline | 1 | 0 | 50000 | 50000 | 0 | 0 | 3734317.08 | 0.09 | 0.09 | 0.11 | 0.13 |
| rpc_add_c1 | 1 | 8 | 500 | 500 | 0 | 0 | 6822.67 | 144.95 | 105.62 | 319.93 | 970.66 |
| rpc_add_c2 | 2 | 8 | 1000 | 1000 | 0 | 0 | 17838.04 | 105.11 | 89.34 | 184.17 | 373.73 |
| rpc_add_c4 | 4 | 8 | 2000 | 2000 | 0 | 0 | 18694.38 | 188.38 | 166.74 | 338.21 | 748.85 |
| rpc_add_c8 | 8 | 8 | 3200 | 3200 | 0 | 0 | 19892.11 | 369.92 | 345.30 | 664.15 | 951.86 |
| rpc_echo_64b_c4 | 4 | 64 | 1200 | 1200 | 0 | 0 | 18180.91 | 191.34 | 167.86 | 335.24 | 573.37 |
| rpc_echo_1024b_c4 | 4 | 1024 | 800 | 800 | 0 | 0 | 16725.94 | 209.34 | 189.76 | 351.26 | 791.41 |
| rpc_echo_4096b_c4 | 4 | 4096 | 400 | 400 | 0 | 0 | 14094.19 | 268.32 | 247.61 | 419.41 | 768.72 |
| rpc_delay_overload_c16 | 16 | 4 | 160 | 100 | 60 | 0 | 98.28 | 97515.85 | 100984.52 | 101680.62 | 102476.92 |

## Analysis

- Small-payload latency stays below 1 ms at p99 in all `Add` scenarios, which is good enough for a resume-grade demo RPC stack and easy to explain in interviews.
- Throughput rises from concurrency 1 to 8, showing the multi-threaded server and connection reuse are working; average latency grows with concurrency as expected on a 2-core environment.
- `Echo` throughput drops from 64 B to 4 KB because serialization, copy cost, and kernel send/recv overhead become dominant as payload size increases.
- The overload run finishes with `62.5%` success and `37.5%` fast-reject responses, demonstrating the bounded queue design is effective.
- During implementation, enabling `TCP_NODELAY` and merging header/body into one send removed the classic small-packet latency spike caused by Nagle plus delayed ACK, which is also a good optimization story for interviews.

## Reproduce

```bash
cmake -S . -B build
cmake --build build -j 2
./build/benchmark_rpc
python3 tools/render_benchmark_report.py \
  --input benchmarks/results/latest.csv \
  --output docs/perf-report.md \
  --cpu "Intel(R) Xeon(R) Platinum 8336C CPU @ 2.30GHz" \
  --compiler "g++ (Ubuntu 13.3.0-6ubuntu2~24.04) 13.3.0" \
  --kernel "Linux 6.8.0-55-generic #57-Ubuntu SMP PREEMPT_DYNAMIC Wed Feb 12 23:42:21 UTC 2025 x86_64 GNU/Linux" \
  --cores 2 \
  --date "2026-03-19"
```
