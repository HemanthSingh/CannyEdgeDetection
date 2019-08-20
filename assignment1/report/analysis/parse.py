myFile = open('output','r')
myFileOut = open('data','w')

#iteration = None

for i,l in enumerate(myFile):
    myLine = None

    if l.startswith('Testing'):
        x = l.split(' ')
        #iteration = x[2]
        myFileOut.write(x[2].rstrip() + ',')
    elif l.startswith('Total execution time ='):
        x = l.split(' ')
        myFileOut.write(x[4] + ',')
    elif l.startswith('Serial execution time '):
        x = l.split(' ')
        myFileOut.write(x[4] + ',')
    elif l.startswith('Neon execution time '):
        x = l.split(' ')
        myFileOut.write(x[4] + '\n')
        
        

myFileOut.close()
