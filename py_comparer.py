import subprocess
import time
import json
from multiprocessing import Pool
import os
import sys

class Player(object):
    count = 0;
    def __init__ (self, start, name=''):
        self.start = start
        self.number = Player.count
        self.port = 31001+self.number
        Player.count+=1
        self.wins=0
        self.last_score=0
        self.total_score=0
        self.first_places=0
        self.cur_first_place=0
        self.cur_place=0
        self.cur_damage=0
        self.cur_kills=0
        self.sum_place=0
        self.sum_damage=0
        self.sum_kills=0

        if name:
            self.name = name
        else:
            self.name=f'p{Player.number}'

numberIterations=2

players=[]
players.append(Player(r'/home/yuliy/projects/raic-2022/build/ai_cup_22', 'P_1'))

player_bace_TCP ={
    "host": None,
    "port": 31001,
    "accept_timeout": None,
    "single_timeout": None,
    "total_time_limit": None,
    "token": None,
    "run": None
}

config_bace = {
  "seed": None,
  "game": {
    "Create": "Round1"
  },
  "players": []
}

def runProc(i):
    players_for_conf=[]
    config_dict= config_bace.copy()
    for p in players:
        pc = player_bace_TCP.copy()
        pc['port'] = p.port+i*10
        players_for_conf.append({'Tcp':pc})
    config_dict["players"] = players_for_conf
    conf_name=f'paralel_conf_{i}.json'
    results_name = f'result_parallel_{i}.txt'
    with open(conf_name, 'w') as f:
        json.dump(config_dict, f)

    server=subprocess.Popen(f'/home/yuliy/projects/raic-2022/app-linux/aicup22 --config {conf_name} --save-results {results_name} --batch-mode' , shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)

    time.sleep(2)

    clients = []
    for p in players:
        print("Run client for ", p.name)
        clients.append(subprocess.Popen(f'{p.start} 127.0.0.1 {p.port+i*10}', shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT))

    result=True
    countDrop=0
    while(result):
        #line = p1.stdout.readline()
        #if line : print(line)
        line=server.stdout.readline()
        if 'Dropping' in str(line):
            countDrop+=1
        if countDrop==len(players):
            result=False


    time.sleep(1)
    print(f"Match {i+1} ends")

    server.terminate()
    for c in clients:
        c.terminate()

    os.remove(conf_name)
    return (i, results_name)

def calculate_results(list_results):
    for (i, results_name) in list_results:
        with open(results_name) as fp:
                data=json.load(fp)
        for p in players:
            p.last_score = data['results']['players'][p.number]['score']
            p.total_score+=p.last_score
            p.cur_first_place = 0
            p.cur_place = data['results']['players'][p.number]['place']
            if p.cur_place==1: p.cur_first_place=1
            p.first_places+=p.cur_first_place
            p.sum_place+=p.cur_place
            p.cur_damage=data['results']['players'][p.number]['damage']
            p.sum_damage+=p.cur_damage
            p.cur_kills=data['results']['players'][p.number]['kills']
            p.sum_kills+=p.cur_kills
        winner = max(players, key=lambda x: x.last_score)
        winner.wins+=1

        print(f"match number {i+1} from {numberIterations}")

        print('Name\t'+'\t'.join([f'{_p.name}' for _p in players]))
        print('Score\t'+'\t'.join([f'{int(_p.last_score)} ' for _p in players]))
        print('Place\t'+'\t'.join([f'{int(_p.cur_place)} ' for _p in players]))
        print('Damage\t'+'\t'.join([f'{int(_p.cur_damage)} ' for _p in players]))
        print('Kills\t'+'\t'.join([f'{int(_p.cur_kills)} ' for _p in players]))
        print('S_Wins\t'+'\t'.join([f'{int(_p.wins)} ' for _p in players]))
        print('S_First\t'+'\t'.join([f'{int(_p.first_places)}' for _p in players]))
        os.remove(results_name)

    print(f'\nFinal Results {numberIterations} matches')
    print('Name\t\t'+'\t'.join([f'{_p.name}' for _p in players]))
    print('Score\t\t'+'\t'.join([f'{int(_p.total_score)} ' for _p in players]))
    print('Wins\t\t'+'\t'.join([f'{int(_p.wins)} ' for _p in players]))
    print('First\t\t'+'\t'.join([f'{int(_p.first_places)} ' for _p in players]))
    print('Avg_Score\t'+'\t'.join([f'{int(_p.total_score/numberIterations)} ' for _p in players]))
    print('Avg_Damage\t'+'\t'.join([f'{int(_p.sum_damage/numberIterations)} ' for _p in players]))
    print('Avg_Kills\t'+'\t'.join([f'{int(_p.sum_kills/numberIterations)} ' for _p in players]))
    print('Avg_Place\t'+'\t'.join([f'{int(_p.sum_place/numberIterations)} ' for _p in players]))

if __name__ == '__main__':
    if len(sys.argv)>0:
        try:
            numberIterations = int(sys.argv[1])
        except ValueError:
            print("Can't parse number of matches. Use default")
    print(f'Running {numberIterations} matches')

    start_time = time.time()
    with Pool(6) as p:
        calculate_results(p.map(runProc, list(range(numberIterations))))
    time_prog = time.time()-start_time
    print(f"{numberIterations} matches. {int(time_prog//60)}:{int(time_prog%60)}")


