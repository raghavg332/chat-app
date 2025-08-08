import asyncio
import argparse
import time
import random
import string
from statistics import median

# --------------------- Metrics ---------------------
class Reservoir:
    def __init__(self, k=200000):
        self.k = k
        self.samples = []
        self.n = 0
        random.seed(42)

    def add(self, x):
        self.n += 1
        if len(self.samples) < self.k:
            self.samples.append(x)
        else:
            j = random.randint(1, self.n)
            if j <= self.k:
                self.samples[j-1] = x

    def percentiles(self, ps=(50, 90, 95, 99)):
        if not self.samples:
            return {p: None for p in ps}
        arr = sorted(self.samples)
        out = {}
        for p in ps:
            idx = min(len(arr)-1, max(0, int(round((p/100)*(len(arr)-1)))))
            out[p] = arr[idx]
        return out

class Metrics:
    def __init__(self, reservoir_k=200000):
        self.connections_ok = 0
        self.connections_failed = 0
        self.bytes_sent = 0
        self.bytes_recv = 0
        self.requests_sent = 0
        self.responses_observed = 0
        self.latencies = Reservoir(reservoir_k)

    def summary(self, duration_s):
        p = self.latencies.percentiles()
        return {
            "connections_ok": self.connections_ok,
            "connections_failed": self.connections_failed,
            "requests_sent": self.requests_sent,
            "responses_observed": self.responses_observed,
            "ttfb_ms_p50": None if p[50] is None else round(1000*p[50], 2),
            "ttfb_ms_p90": None if p[90] is None else round(1000*p[90], 2),
            "ttfb_ms_p95": None if p[95] is None else round(1000*p[95], 2),
            "ttfb_ms_p99": None if p[99] is None else round(1000*p[99], 2),
            "bytes_sent": self.bytes_sent,
            "bytes_recv": self.bytes_recv,
            "duration_s": round(duration_s, 2),
        }

# --------------------- Client ---------------------
class Client:
    def __init__(self, idx, host, port, req_rate, metrics: Metrics, start_evt: asyncio.Event, stop_evt: asyncio.Event):
        self.idx = idx
        self.host = host
        self.port = port
        self.req_rate = req_rate
        self.metrics = metrics
        self.start_evt = start_evt
        self.stop_evt = stop_evt
        self.reader = None
        self.writer = None
        self.running = True
        self.pending_probe_started_at = None  # timestamp of last /users request

    async def connect(self):
        try:
            self.reader, self.writer = await asyncio.open_connection(self.host, self.port)
            self.metrics.connections_ok += 1
            return True
        except Exception:
            self.metrics.connections_failed += 1
            return False

    async def login(self):
        # Drain any prompt/welcome bytes (best-effort with timeout)
        for _ in range(3):
            try:
                data = await asyncio.wait_for(self.reader.read(4096), timeout=0.05)
                if not data:
                    break
                self.metrics.bytes_recv += len(data)
                # If server keeps the connection open and there's more to read later,
                # these reads will just time out quickly; that's fine.
            except asyncio.TimeoutError:
                break
            except Exception:
                break

        uname = f"bot_{self.idx}"
        try:
            self.writer.write(uname.encode())   # no newline; matches your server behavior
            await self.writer.drain()
            self.metrics.bytes_sent += len(uname)
        except Exception:
            self.running = False

    async def reader_loop(self):
        while self.running and not self.stop_evt.is_set():
            try:
                # Read whatever arrives; treat *first byte arrival* after a probe as TTFB
                data = await self.reader.read(4096)
                if not data:
                    self.running = False
                    break
                self.metrics.bytes_recv += len(data)
                if self.pending_probe_started_at is not None:
                    ttfb = time.time() - self.pending_probe_started_at
                    self.metrics.latencies.add(ttfb)
                    self.metrics.responses_observed += 1
                    self.pending_probe_started_at = None
            except Exception:
                self.running = False
                break

    async def sender_loop(self):
        await self.start_evt.wait()
        if self.req_rate <= 0:
            # Idle client: just sit until stop
            await self.stop_evt.wait()
            return

        interval = 1.0 / self.req_rate
        next_send = time.time()
        while self.running and not self.stop_evt.is_set():
            now = time.time()
            if now >= next_send:
                try:
                    # Send /users command (no newline)
                    self.writer.write(b"/users")
                    await self.writer.drain()
                    self.metrics.bytes_sent += len(b"/users")
                    self.metrics.requests_sent += 1
                    # Start latency probe: TTFB measured on next read arrival
                    self.pending_probe_started_at = time.time()
                except Exception:
                    self.running = False
                    break
                next_send += interval
            # Small sleep to avoid busy loop
            await asyncio.sleep(min(0.005, max(0.0, next_send - time.time())))

    async def run(self):
        ok = await self.connect()
        if not ok:
            return
        await self.login()
        reader_task = asyncio.create_task(self.reader_loop())
        sender_task = asyncio.create_task(self.sender_loop())
        await asyncio.wait([reader_task, sender_task], return_when=asyncio.FIRST_COMPLETED)
        # Cleanup
        try:
            self.writer.close()
            await self.writer.wait_closed()
        except Exception:
            pass

# --------------------- Single run ---------------------
async def run_once(host, port, clients, rate, duration, reservoir_k):
    metrics = Metrics(reservoir_k)
    start_evt = asyncio.Event()
    stop_evt = asyncio.Event()

    cobjs = [Client(i, host, port, rate, metrics, start_evt, stop_evt) for i in range(clients)]

    # Connect + login
    await asyncio.gather(*(c.connect() for c in cobjs))
    # keep only connected
    cobjs = [c for c in cobjs if c.reader is not None]
    await asyncio.gather(*(c.login() for c in cobjs))

    # Launch loops
    tasks = [asyncio.create_task(c.run()) for c in cobjs]

    # Start and run duration
    test_start = time.time()
    start_evt.set()
    try:
        await asyncio.sleep(duration)
    except asyncio.CancelledError:
        pass
    stop_evt.set()

    # Wait a bit for clean shutdown
    await asyncio.gather(*tasks, return_exceptions=True)
    test_end = time.time()

    return metrics.summary(test_end - test_start)

# --------------------- Sweep mode ---------------------
async def run_sweep(host, port, start, step, stop, rate, duration, reservoir_k,
                    max_fail_pct=0.0, max_p50_ms=None):
    results = []
    peak_clients = start
    for n in range(start, stop + 1, step):
        summary = await run_once(host, port, n, rate, duration, reservoir_k)
        results.append((n, summary))
        fail_pct = 100.0 * summary["connections_failed"] / max(1, (summary["connections_ok"] + summary["connections_failed"]))
        p50 = summary["ttfb_ms_p50"] or 0.0
        ok_fail = fail_pct <= max_fail_pct
        ok_p50 = True if (max_p50_ms is None or p50 <= max_p50_ms) else False
        print(f"\n[SWEEP] clients={n} ok={ok_fail and ok_p50} "
              f"fail%={fail_pct:.2f} p50={p50}ms "
              f"req={summary['requests_sent']} resp={summary['responses_observed']}")
        if ok_fail and ok_p50:
            peak_clients = n
        else:
            break
    return results, peak_clients

# --------------------- CLI ---------------------
def parse_args():
    ap = argparse.ArgumentParser(description="Load tester for chat server (no framing) using /users TTFB")
    ap.add_argument("--host", default="13.200.235.191")
    # ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=8081)
    ap.add_argument("--clients", type=int, default=100, help="Clients for single run")
    ap.add_argument("--rate", type=float, default=0.5, help="Requests per second per client (/users)")
    ap.add_argument("--duration", type=int, default=30, help="Seconds")
    ap.add_argument("--reservoir", type=int, default=200000)
    ap.add_argument("--sweep", action="store_true", help="Enable sweep mode")
    ap.add_argument("--sweep-start", type=int, default=50)
    ap.add_argument("--sweep-step", type=int, default=50)
    ap.add_argument("--sweep-stop", type=int, default=500)
    ap.add_argument("--max-fail-pct", type=float, default=0.0, help="Abort sweep if connection failure %% exceeds this")
    ap.add_argument("--max-p50-ms", type=float, default=None, help="Abort sweep if p50 TTFB exceeds this (ms)")
    return ap.parse_args()

async def main_async():
    args = parse_args()
    if args.sweep:
        results, peak = await run_sweep(
            args.host, args.port,
            args.sweep_start, args.sweep_step, args.sweep_stop,
            args.rate, args.duration, args.reservoir,
            max_fail_pct=args.max_fail_pct, max_p50_ms=args.max_p50_ms
        )
        print("\n=== SWEEP RESULTS ===")
        for n, s in results:
            print(f"clients={n} p50={s['ttfb_ms_p50']} p90={s['ttfb_ms_p90']} "
                  f"ok={s['connections_ok']} fail={s['connections_failed']}")
        print(f"\nPeak stable clients (per criteria): {peak}")
    else:
        s = await run_once(args.host, args.port, args.clients, args.rate, args.duration, args.reservoir)
        print("\n=== RUN SUMMARY ===")
        for k, v in s.items():
            print(f"{k}: {v}")

if __name__ == "__main__":
    try:
        asyncio.run(main_async())
    except KeyboardInterrupt:
        pass