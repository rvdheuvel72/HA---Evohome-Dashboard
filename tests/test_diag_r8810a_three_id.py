"""Offline diagnostics regression tests; no real RF, network or boiler access."""
import tempfile
from pathlib import Path
import unittest

import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
from diag_r8810a_three_id import Gateway, GatewayError, extract_3220, run, one_read, report_text, IDS

TSV = '''# RAMSES RX capture: test fixture
sequence\tlocal_time\tutc_epoch_ms\tuptime_ms\ttype\trssi_magnitude\tmessage
1\t2026-11-01 10:00:00\t1793523600000\t10100\tpacket\t70\t096 RQ --- 01:245914 10:048994 --:------ 3220 005 0000120000
2\t2026-11-01 10:00:00\t1793523600190\t10290\tpacket\t60\t094 RP --- 10:048994 01:245914 --:------ 3220 005 00C01201CD
3\t2026-11-01 10:00:16\t1793523616000\t26000\tpacket\t60\t094 RP --- 10:048994 01:245914 --:------ 3220 005 00C0193800
4\t2026-11-01 10:00:32\t1793523632000\t42000\tpacket\t90\t094 RP --- 10:048994 01:245914 --:------ 3220 005 00F0750000
5\t2026-11-01 10:00:33\t1793523633000\t43000\trejected\t0\tparity error
6\t2026-11-01 10:00:34\t1793523634000\t44000\tpacket\t75\t094 RP --- 10:048994 01:245914 --:------ 3220 005 00C0270000
'''


class FakeGateway:
    def __init__(self, *, existing=False, fault=None):
        self.existing = existing
        self.actions = []
        self.requests = []
        self.current_id = 25
        self.fault = fault
        self.started = False
        self.uptime = 10000

    def get_json(self, path):
        if path.endswith('/capture/status'):
            return {"active":self.started,"count":1 if self.existing else (6 if self.started else 0),"full":False}
        if path.endswith('/status'):
            self.uptime += 500
            return {"uptime_ms":self.uptime}
        if path.endswith('/paired-read'):
            data_id = self.current_id
            return {"ready": True, "controller_id":245914,"bridge_id":48994,
                    "data_id": data_id,"state": "timeout" if data_id==117 else "read_ack",
                    "raw": 460,"has_value":data_id!=117,"value":1.8,"tx_phase":"local_complete",
                    "tx_local_complete":True,"tx_bytes":44,"tx_expected_bytes":44}
        raise AssertionError(path)

    def post(self, path, fields, config_header):
        self.requests.append((path,fields,config_header))
        if self.fault and len([x for x in self.requests if x[0].endswith('/paired-read')])==2:
            raise GatewayError("simulated timeout")
        if path.endswith('/paired-read'):
            self.current_id = int(fields['data_id'])
            return {"queued":True,"read_only":True}
        raise AssertionError(path)

    def capture_action(self, action):
        self.actions.append(action)
        self.started = action=='start'
        return {"ok":True}

    def request(self, method, path):
        if path.endswith('/capture/download'):
            return TSV
        raise AssertionError(path)


class DiagnosticTests(unittest.TestCase):
    def test_ids_and_rf_decoder(self):
        self.assertEqual(IDS, (18,25,117))
        frame = extract_3220(TSV)
        self.assertEqual([(x['verb'],x['id']) for x in frame],
                         [('RQ',18),('RP',18),('RP',25),('RP',117)])
        self.assertEqual(frame[-1]['ot_reply'], 'unknown_data_id')
        self.assertEqual(frame[1]['source'], '10:048994')

    def test_one_shot_restricts_data_ids_and_transmit_intent(self):
        fake = FakeGateway()
        with self.assertRaises(ValueError):
            one_read(fake,0)
        self.assertEqual(fake.requests,[])
        result = one_read(fake,18)
        self.assertEqual(result['state'],'read_ack')
        self.assertEqual(result['tx_phase'],'local_complete')
        self.assertEqual(fake.requests[0][1]['confirm'],
                         'one-read-own-r8810a-as-observed-controller')
        self.assertEqual(fake.requests[0][2], 'paired-bridge-read')

    def test_produces_three_ids_capture_and_inconclusive_timeout(self):
        with tempfile.TemporaryDirectory() as tmp:
            output=Path(tmp)/'run'
            fake=FakeGateway()
            result=run(fake,output)
            self.assertEqual([x['id'] for x in result['reads']],[18,25,117])
            self.assertEqual(result['reads'][-1]['state'],'timeout')
            self.assertEqual(fake.actions,['start','stop'])
            self.assertEqual([x[1]['data_id'] for x in fake.requests],['18','25','117'])
            self.assertEqual(result['reads'][0]['causality'],
                             'unverified: shared physical Evohome RF identity')
            self.assertTrue(result['reads'][0]['matching_rx_candidates'])
            self.assertTrue(result['reads'][0]['natural_rq_candidates'])
            self.assertTrue((output/'capture.tsv').exists())
            self.assertTrue((output/'report.json').exists())
            doc=(output/'report.md').read_text()
            self.assertIn('Timeout means inconclusive, NOT unsupported.',doc)
            self.assertIn('ID',report_text(result))

    def test_preserves_existing_capture_by_default(self):
        with tempfile.TemporaryDirectory() as tmp:
            fake=FakeGateway(existing=True)
            output=Path(tmp)/'run'
            with self.assertRaisesRegex(GatewayError,'Existing capture'):
                run(fake,output)
            self.assertFalse(output.exists())
            self.assertEqual(fake.requests,[])
            self.assertEqual(fake.actions,[])

    def test_interrupt_writes_partial_evidence_and_stops_capture(self):
        with tempfile.TemporaryDirectory() as tmp:
            fake=FakeGateway(fault=True)
            result=run(fake,Path(tmp)/'partial')
            self.assertTrue(result['error'])
            self.assertEqual(len(result['reads']),1)
            self.assertEqual(fake.actions,['start','stop'])
            self.assertTrue((Path(tmp)/'partial'/'capture.tsv').exists())

    def test_denies_unsafe_gateway_url(self):
        for url in ('https://example.org','http://user:pass@host','http://host/path',
                    'ftp://example.org','file:///etc/passwd'):
            with self.subTest(url=url):
                with self.assertRaises(ValueError):
                    Gateway(url,'test','secret')


if __name__ == '__main__':
    unittest.main()