import unittest
from analyze_ch0_trace import analyze


def fixture():
    rows = []
    for i, (seq, pv, valid, count, avg, accepted) in enumerate([
        (1,100,1,1,100,1),(2,200,1,2,150,1),(3,300,1,3,200,1),
        (4,400,1,4,250,1),(5,500,1,5,300,1),(6,600,1,5,400,1),
        (6,999,1,5,400,0),(6,0,0,0,0,0),(6,-40,1,1,-40,1)]):
        rows.append(dict(init_ms=0,now_ms=(i+1)*20,elapsed_ms=20,lateness_ms=0,
            sequence=seq,pv=pv,provider_ok=1,input_valid=valid,accepted=accepted,
            sample_count=count,output_valid=int(count>0),average=avg,status=0,
            cycles=100,cycles_valid=1))
    return rows


class AnalyzerTests(unittest.TestCase):
    def test_complete(self):
        r = analyze(fixture(), 100000000, 10, 10)
        self.assertEqual((r['functional'],r['timing']), ('PASS','PASS'))
    def test_no_timing(self):
        self.assertEqual(analyze(fixture())['timing'], 'INCOMPLETE')
    def test_missing_coverage(self):
        self.assertEqual(analyze(fixture()[:2])['functional'], 'INCOMPLETE')
    def test_corrupt_average(self):
        rows=fixture(); rows[5]['average']=300
        self.assertTrue(analyze(rows)['failures'])
    def test_missing_row(self):
        rows=fixture(); del rows[2]
        self.assertTrue(analyze(rows)['failures'])
    def test_budget(self):
        self.assertTrue(analyze(fixture(),1000000,10,10)['failures'])
    def test_missing_cycles(self):
        rows=fixture(); rows[2]['cycles_valid']=0
        self.assertEqual(analyze(rows,1000000,1000,10)['timing'],'INCOMPLETE')


if __name__ == '__main__':
    unittest.main()
