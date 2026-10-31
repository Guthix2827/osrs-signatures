"""Deterministic HTTP integration test; supply TEST_SERVER_BINARY=.../memory-service-tests."""
import concurrent.futures, json, os, struct, subprocess, tempfile, time
from pathlib import Path
import urllib.error, urllib.request

def request(path, data=None, headers=None):
    req=urllib.request.Request('http://127.0.0.1:18081'+path,
        data=None if data is None else json.dumps(data).encode(),
        headers={'Content-Type':'application/json', **(headers or {})})
    try: response=urllib.request.urlopen(req,timeout=90)
    except urllib.error.HTTPError as error: response=error
    with response: return response.status,dict(response.headers),response.read()

with tempfile.TemporaryDirectory() as temporary:
    directory=Path(temporary)
    fixture=directory/'stats.json'; clock=directory/'clock.txt'
    names=['attack','strength','defence','hitpoints','ranged','prayer','magic','cooking',
        'woodcutting','fletching','fishing','firemaking','crafting','smithing','mining',
        'herblore','agility','thieving','slayer','farming','runecraft','hunter','construction','sailing']
    stats={'name':'Zezima','fetchedAt':0,'totalLevel':2376,'overallRank':None,
        'skills':{name:{'level':99,'xp':200000000,'rank':1} for name in names}}
    fixture.write_text(json.dumps(stats)); clock.write_text('100')
    log=(directory/'server.log').open('w')
    def start():
        process=subprocess.Popen([os.environ['TEST_SERVER_BINARY'],str(fixture),str(clock)],cwd=Path(__file__).resolve().parents[1],stdout=log,stderr=log)
        for _ in range(100):
            try:
                if request('/health')[0]==200: return process
            except OSError: pass
            if process.poll() is not None: raise RuntimeError((directory/'server.log').read_text())
            time.sleep(.05)
        raise RuntimeError('Test server did not start')
    def stop(process):
        process.terminate()
        try: process.wait(10)
        except subprocess.TimeoutExpired: process.kill();process.wait()
    process=start()
    try:
        url='/signatures/Lynx%20Titan.png?design=classic&background=blue'
        status,headers,first=request(url)
        assert status==200,first
        assert first[:8]==b'\x89PNG\r\n\x1a\n' and struct.unpack('>II',first[16:24])==(320,125)
        etag=headers['ETag'];assert 's-maxage=60' in headers['Cache-Control']
        assert request(url,headers={'If-None-Match':etag})[0]==304
        assert request(url,headers={'If-None-Match':'W/'+etag})[0]==304
        assert 'attachment' in request(url+'&download=1')[1]['Content-Disposition']
        assert request('/snapshots/anything.png')[0]==404
        assert request('/signatures/Zezima.png?design=unpublished')[0]==400
        clock.write_text('160');stats['skills']['attack']['rank']=10;fixture.write_text(json.dumps(stats))
        assert request(url,headers={'If-None-Match':etag})[0]==304
        clock.write_text('220');stats['skills']['attack']['level']=75;fixture.write_text(json.dumps(stats))
        with concurrent.futures.ThreadPoolExecutor(max_workers=6) as pool:
            replies=list(pool.map(lambda _:request(url),range(6)))
        assert all(reply[0]==200 and reply[2]==replies[0][2] for reply in replies)
        assert replies[0][1]['ETag']!=etag
        current,new_etag=replies[0][2],replies[0][1]['ETag']
        status,_,body=request('/api/signatures',{'name':'Lynx Titan','design':'classic'})
        ready=json.loads(body);assert status==200 and ready['imageUrl']==url
        assert 'snapshotUrl' not in ready
        stop(process);process=start()
        status,headers,regenerated=request(url)
        assert status==200 and regenerated==current and headers['ETag']==new_etag
        clock.write_text('280');stats['skills']['hitpoints']['level']=None;fixture.write_text(json.dumps(stats))
        assert request(url)[0]==200
        print('PASS: cold GET, PNG, ETag/304, download, concurrency, changed stats, restart and missing combat stats')
    finally: stop(process);log.close()
