#!/usr/bin/env python3
"""Run one make DAG node with observable subprocess timing and failure receipts."""
import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import re
import subprocess
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('task')
    parser.add_argument('command', nargs=argparse.REMAINDER)
    args = parser.parse_args()
    command = args.command[1:] if args.command[:1] == ['--'] else args.command
    if not re.fullmatch(r'[a-z][a-z0-9-]*', args.task) or not command:
        parser.error('safe task name and command required')
    directory = Path(os.environ.get('ENCORE_CONTENT_LOG_DIR', 'build/content-jobs'))
    directory.mkdir(parents=True, exist_ok=True)
    receipt = directory / (args.task + '.json')
    receipt.unlink(missing_ok=True)
    start = time.time()
    with (directory / (args.task + '.log')).open('wb') as log:
        process = subprocess.Popen(command, stdout=log, stderr=subprocess.STDOUT)
        print('CONTENT START {} pid={} at={}'.format(args.task, process.pid,
            datetime.fromtimestamp(start, timezone.utc).isoformat()), flush=True)
        code = process.wait()
    end = time.time()
    value = dict(task=args.task, pid=process.pid, run_id=os.environ.get('ENCORE_CONTENT_RUN_ID'), started=start, finished=end,
                 seconds=end-start, returncode=code, command=command)
    temporary = receipt.with_suffix('.tmp')
    temporary.write_text(json.dumps(value, indent=2)+'\n', encoding='utf-8')
    temporary.replace(receipt)
    print((directory / (args.task + '.log')).read_text(encoding='utf-8', errors='replace'), end='', flush=True)
    print('CONTENT END {} pid={} status={} seconds={:.3f}'.format(
        args.task, process.pid, code, end-start), flush=True)
    return code if code >= 0 else 1


if __name__ == '__main__':
    raise SystemExit(main())
