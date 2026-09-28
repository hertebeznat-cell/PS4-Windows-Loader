import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from check_cpu_log import parse, summarize  # noqa: E402


def entry(leaf, eax=0, ecx=0, edx=0):
    fields = (("leaf", leaf), ("subleaf", 0), ("eax", eax),
              ("ebx", 0), ("ecx", ecx), ("edx", edx))
    return "\n".join(f"CPU48: {field}=0x{value:016X}" for field, value in fields)


class CpuLogTest(unittest.TestCase):
    def log(self, *entries):
        return "\n".join(("CPU48: CPUID raw registers begin", *entries,
                          "CPU48: CPUID raw registers end"))

    def test_flags_and_missing_leaf_are_distinct(self):
        sample = self.log(
            entry(0, eax=1), entry(1, ecx=(1 << 13) | (1 << 23)),
            entry(0x80000000, eax=0x80000001),
            entry(0x80000001, ecx=1 << 8, edx=1 << 20),
        )
        result = dict(summarize(parse(sample)))
        self.assertEqual(result["CMPXCHG16B"], "YES")
        self.assertEqual(result["SSE4.2"], "NO")
        self.assertEqual(result["NPT (SLAT)"], "UNKNOWN")
        self.assertEqual(result["PREFETCHW"], "YES")

    def test_incomplete_capture_is_rejected(self):
        with self.assertRaises(ValueError):
            parse("CPU48: CPUID raw registers begin\nCPU48: leaf=0x0000000000000000")

    def test_inconsistent_max_leaf_is_rejected(self):
        sample = self.log(entry(0, eax=1), entry(0x80000000, eax=0x80000001))
        with self.assertRaises(ValueError):
            parse(sample)

    def test_standalone_log_requires_finish_marker(self):
        sample = "MODE: CPU_ONLY; no EFI or Boot Manager entry\n" + self.log(
            entry(0, eax=0), entry(0x80000000, eax=0x80000000)
        )
        with self.assertRaisesRegex(ValueError, "did not finish"):
            parse(sample)
        self.assertIn((0, 0), parse(sample + "\nCPU probe finished\n"))


if __name__ == "__main__":
    unittest.main()
