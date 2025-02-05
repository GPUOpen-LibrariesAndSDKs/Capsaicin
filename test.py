import flip_evaluator as flip
import matplotlib.pyplot as plt
import glob
import csv
import locale
if __name__ == '__main__':
    
    ref = "dump/pngs/Sponza_KiaraDawn_Main_ReferencePathTracer_4692_0.004851.png"


    test = "dump/pngs/Sponza_KiaraDawn_Main_GI-1.1_6047_0.005431.png"
    
    flipErrorMap, meanFLIPError, parameters = flip.evaluate(ref, test, "HDR")

    plt.imshow(flipErrorMap)
    #plt.show()
    plt.savefig("dump/"+"Main"+"_"+str(round(meanFLIPError, 6))+".png")
    #files = glob.glob ("dump/*.jpeg")
        
    #for myFile in files:
    #    flipErrorMap, meanFLIPError, parameters = flip.evaluate(ref, myFile, "HDR")
        #print("Mean FLIP error: ", round(meanFLIPError, 6), "\n")
        
    #    plt.imshow(flipErrorMap)

        #writer.writerow(numbers)
    #    plt.savefig("dump/"+fileName+"_"+str(round(meanFLIPError, 6))+".jpeg")
    #with open('flipresults.csv', 'w', newline='') as file:
    #    writer = csv.writer(file)
    #    writer.writerow(["Step Size","Sun Step Size","Density Threshold","Mean Error"])
    #    
    

    
    


