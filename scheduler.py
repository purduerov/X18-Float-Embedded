import time

class Scheduler:
    def __init__(self):
        self.tasks = []
        self.rates = []
        self.execs = []
        
        self.timer = 0
        
    def add_task(self, task, rate):
        self.tasks.append(task)
        self.rates.append(rate)
        self.execs.append(0)
        
    def tick(self):
        for i,task in enumerate(self.tasks):
            if (self.timer - self.execs[i] >= self.rates[i]):
                task()
                self.execs[i] = self.timer
        self.timer += 1