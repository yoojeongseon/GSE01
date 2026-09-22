"""Summarize renderer CSV sessions without third-party packages or running the game."""
import argparse
import csv
import math
from pathlib import Path
from statistics import mean


def percentile(values, fraction):
    values = sorted(values)
    return values[max(0, math.ceil(len(values) * fraction) - 1)]


def analyze(path, warmup):
    with path.open(encoding="utf-8-sig", newline="") as stream:
        rows = list(csv.DictReader(stream))
    if not rows:
        raise ValueError("CSV contains no completed frames")
    required = {"schema", "segment", "frame", "seconds", "frame_ms", "draws", "gpu_frame",
                "gpu_ms", "world_draws", "ui_draws", "post_draws", "legacy_geometry_runs",
                "useful_vertices", "submitted_vertices", "upload_bytes", "render_cpu_ms",
                "update_cpu_ms", "swap_cpu_ms", "last_log_flush_ms", "upload_cpu_ms",
                "submit_cpu_ms", "buffer_growths", "queue_growths", "glyph_misses",
                "mesh_generations", "gpu_query_skipped", "batching", "scene"}
    if not required.issubset(rows[0]):
        raise ValueError("Unsupported or incomplete CSV schema")
    numeric = required - {"scene"}
    for row in rows:
        for key in numeric:
            row[key] = float(row[key])
            if not math.isfinite(row[key]):
                raise ValueError(f"Non-finite {key}")
        if row["schema"] != 1:
            raise ValueError("Unsupported schema version")

    # Timing results are delayed and can be repeated. Attribute once to the source frame,
    # never to the later frame/scene that happened to collect them.
    gpu_by_frame = {}
    for row in rows:
        if row["gpu_ms"] >= 0 and row["gpu_frame"] > 0:
            gpu_by_frame[int(row["gpu_frame"])] = row["gpu_ms"]
    segments = {}
    for row in rows:
        segments.setdefault(int(row["segment"]), []).append(row)
    lines = [f"# 렌더링 분석: {path.name}", "",
             f"완료 프레임 {len(rows):,}개. 설정 구간별 초기 {warmup:g}초를 제외한다.", "",
             "GPU 시간은 gpu_frame으로 원래 프레임에 연결하고 중복을 제거한다. "
             "FPS는 실제 프레임 간격 합계로 계산하며 CPU/GPU 시간의 합이 아니다.", ""]
    for segment, all_samples in segments.items():
        first = all_samples[0]
        samples = [r for r in all_samples if r["seconds"] - first["seconds"] >= warmup]
        lines += [f"## 구간 {segment}: {first['scene']} / batching={int(first['batching'])}", "",
                  f"화면 {first.get('width')}×{first.get('height')}, HDR={first.get('hdr')}, "
                  f"Bloom={first.get('bloom')}, edge blur={first.get('edge_blur')}, "
                  f"vignette={first.get('vignette')}, exposure={first.get('exposure')}", ""]
        if not samples:
            lines += ["워밍업 이후 데이터가 없다. 더 오래 측정하거나 --warmup-seconds를 줄인다.", ""]
            continue
        n = len(samples)
        total = sum(r["frame_ms"] for r in samples)
        if total <= 0:
            raise ValueError("Non-positive measured duration")
        values = lambda key: [r[key] for r in samples]
        avg = lambda key: mean(values(key))
        gpu = [gpu_by_frame[int(r["frame"])] for r in samples if int(r["frame"]) in gpu_by_frame]
        legacy = avg("legacy_geometry_runs")
        geometry = avg("world_draws") + avg("ui_draws")
        useful = sum(values("useful_vertices"))
        inflation = sum(values("submitted_vertices")) / useful if useful else 0
        lines += ["| 항목 | 측정값 |", "| --- | --- |",
                  f"| 측정 프레임 / 구간 시간 | {n:,} / {total / 1000:.2f}s |",
                  f"| FPS | {n * 1000 / total:.2f} |",
                  f"| 프레임 ms p50 / p95 / p99 | {percentile(values('frame_ms'), .50):.3f} / "
                  f"{percentile(values('frame_ms'), .95):.3f} / {percentile(values('frame_ms'), .99):.3f} |",
                  f"| Draw 평균 / 최대 | {avg('draws'):.2f} / {max(values('draws')):.0f} |",
                  f"| 월드 / UI / 후처리 평균 | {avg('world_draws'):.2f} / {avg('ui_draws'):.2f} / {avg('post_draws'):.2f} |",
                  f"| 기존 방식 geometry run 추정 / 실제 | {legacy:.2f} / {geometry:.2f} |",
                  f"| geometry 호출 감소율 | {(1 - geometry / legacy) * 100 if legacy else 0:.2f}% |",
                  f"| 제출 정점 / 유효 정점 | {inflation:.3f}배 |",
                  f"| 평균 업로드 KiB/frame | {avg('upload_bytes') / 1024:.2f} |",
                  f"| CPU update / render / swap ms | {avg('update_cpu_ms'):.3f} / {avg('render_cpu_ms'):.3f} / {avg('swap_cpu_ms'):.3f} |",
                  f"| CPU upload / GL draw submit ms | {avg('upload_cpu_ms'):.3f} / {avg('submit_cpu_ms'):.3f} |",
                  f"| 이전 로그 flush ms 최대 | {max(values('last_log_flush_ms')):.3f} |",
                  f"| GPU 확보 샘플 | {len(gpu)} / {n} |"]
        if gpu:
            lines += [f"| GPU 평균 / p95 ms | {mean(gpu):.3f} / {percentile(gpu, .95):.3f} |"]
        lines += ["", "관찰:", ""]
        accounting = sum(r['draws'] != r['world_draws'] + r['ui_draws'] + r['post_draws'] for r in samples)
        lines += [f"- 호출 수 합계 불일치: {accounting}프레임.",
                  f"- 메시 생성: {sum(values('mesh_generations')):.0f}회; "
                  f"글리프 최초 생성: {sum(values('glyph_misses')):.0f}회.",
                  f"- GPU 버퍼 용량 증가: {sum(values('buffer_growths')):.0f}회; "
                  f"CPU 큐 용량 증가: {sum(values('queue_growths')):.0f}회.",
                  f"- GPU 측정 생략: {sum(values('gpu_query_skipped')):.0f}프레임."]
        if inflation > 3:
            lines += ["- 정점 패딩 비율이 높다. 긴 동일 메시 구간 분리 임계값과 글리프 표현을 검토한다."]
        lines += ["- FPS만으로 병목을 단정하지 않는다. 타이머·VSync·로그 I/O·장면 변화가 영향을 준다.", ""]
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("csv", type=Path)
    parser.add_argument("--warmup-seconds", type=float, default=5)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    if not math.isfinite(args.warmup_seconds) or args.warmup_seconds < 0:
        parser.error("warmup-seconds must be finite and nonnegative")
    try:
        report = analyze(args.csv, args.warmup_seconds)
    except (OSError, ValueError, KeyError) as error:
        parser.exit(1, f"Could not analyze CSV: {error}\n")
    output = args.output or args.csv.with_name(args.csv.stem + "_analysis.md")
    if output.resolve() == args.csv.resolve():
        parser.error("output must not overwrite the source CSV")
    output.write_text(report, encoding="utf-8")
    print(output)


if __name__ == "__main__":
    main()
