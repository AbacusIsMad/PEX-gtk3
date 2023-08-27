products = ["one", "two", ]

import random

f = [open("t1.txt", "w"), open("t2.txt", "w"), open("t3.txt", "w"), open("t4.txt", "w"), open("t5.txt", "w"), open("t6.txt", "w"), open("t7.txt", "w"), ]

ids = [0 for i in range(len(f))]
orders = [[] for i in range(len(f))]

for i in range(120):
    trader = random.randrange(0, len(f))
    det = random.random()
    if (det < 0.4):
        #buy
        print("BUY {} {} {} {};".format(ids[trader], products[random.randrange(0, len(products))], random.randrange(1, 2000), random.randrange(1, 4)), file=f[trader])
        orders[trader].append(ids[trader])
        ids[trader] += 1
    elif (det < 0.7):
        #buy
        print("SELL {} {} {} {};".format(ids[trader], products[random.randrange(0, len(products))], random.randrange(1, 2000), random.randrange(1, 4)), file=f[trader])
        orders[trader].append(ids[trader])
        ids[trader] += 1
    elif (det < 0.9): #amend
        if (len(orders[trader]) > 0):
            print("AMEND {} {} {};".format(orders[trader][random.randrange(0, len(orders[trader]))], random.randrange(1, 2000), random.randrange(1, 4)), file=f[trader])
        else:
            print("", file=f[trader])
    else: #cancel
        if (len(orders[trader]) > 0):
            can_id = orders[trader][random.randrange(0, len(orders[trader]))]
            orders[trader].remove(can_id)
            print("CANCEL {};".format(can_id), file=f[trader])
        else:
            print("", file=f[trader])
    for j in range(len(f)):
        if j != trader:
            print("", file=f[j]);

for i in range(len(f)):
    for j in range(i):
        print("", file=f[i])
