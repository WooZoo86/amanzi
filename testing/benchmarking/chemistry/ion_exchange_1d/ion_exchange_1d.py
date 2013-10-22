# plots cation concentration along x at last time step 
# benchmark: compares to pflotran simulation results
# author: S.Molins - Sept. 2013

import os
import sys
import h5py
import numpy as np
import matplotlib
matplotlib.use('Agg')
from matplotlib import pyplot as plt


# ----------- AMANZI + ALQUIMIA -----------------------------------------------------------------

def GetXY_Amanzi(path,root,time,comp):

    # open amanzi concentration and mesh files
    dataname = os.path.join(path,root+"_data.h5")
    amanzi_file = h5py.File(dataname,'r')
    meshname = os.path.join(path,root+"_mesh.h5")
    amanzi_mesh = h5py.File(meshname,'r')

    # extract cell coordinates
    y = np.array(amanzi_mesh['0']['Mesh']["Nodes"][0:len(amanzi_mesh['0']['Mesh']["Nodes"])/4,0])
    # y = np.array(amanzi_mesh['Mesh']["Nodes"][0:len(amanzi_mesh['Mesh']["Nodes"])/4,0]) # old style

    # center of cell
    x_amanzi_alquimia  = np.diff(y)/2+y[0:-1]

    # extract concentration array
    c_amanzi_alquimia = np.array(amanzi_file[comp][time]).flatten()
    amanzi_file.close()
    amanzi_mesh.close()
    
    return (x_amanzi_alquimia, c_amanzi_alquimia)

# ----------- PFLOTRAN STANDALONE ------------------------------------------------------------

def GetXY_PFloTran(path,root,time,comp):

    # read pflotran data
    filename = os.path.join(path,"1d-"+root+".h5")
    pfdata = h5py.File(filename,'r')

    # extract coordinates
    y = np.array(pfdata['Coordinates']['X [m]'])
    x_pflotran = np.diff(y)/2+y[0:-1]

    # extract concentrations
    c_pflotran = np.array(pfdata[time][comp]).flatten()
#    c_pflotran = c_pflotran.flatten()
    pfdata.close()

    return (x_pflotran, c_pflotran)

if __name__ == "__main__":

    import os
    import run_amanzi_chem
    import numpy as np

    # root name for problem
    root = "ion-exchange"

    # pflotran
    path_to_pflotran = "pflotran"

     # hardwired for 1d-calcite: time and comp
    times = ['Time:  0.00000E+00 y', 'Time:  5.00000E+01 y',]
    components = ['Na+','Ca++','Mg++','Cl-']

    # comp = 'Total_'+root.title()+' [M]'
    
    u_pflotran = [[[] for x in range(len(components))] for x in range(len(times))]
    for i, time in enumerate(times):
       for j, comp in enumerate(components):          
          x_pflotran, c_pflotran = GetXY_PFloTran(path_to_pflotran,root,time,'Total_'+comp+' [M]')
          u_pflotran[i][j] = c_pflotran
    
    v_pflotran = [[[] for x in range(len(components))] for x in range(len(times))]
    for i, time in enumerate(times):
       for j, comp in enumerate(components):          
          x_pflotran, c_pflotran = GetXY_PFloTran(path_to_pflotran,root,time,'Total_Sorbed_'+comp+' [mol_m^3]')
          v_pflotran[i][j] = c_pflotran

    CWD = os.getcwd()
    local_path = "" 
    
    # subplots
    fig, ax = plt.subplots(2,sharex=True,figsize=(8,8))
    
    
    # hardwired for 1d-exchange:  last time = '71'
    times = ['0','71']
    amanzi_components = ['total_component_concentration.cell.Component 0 conc', \
                         'total_component_concentration.cell.Component 1 conc', \
                         'total_component_concentration.cell.Component 2 conc', \
                         'total_component_concentration.cell.Component 3 conc']
    amanzi_sorbed     = ['total_sorbed.cell.0', \
                         'total_sorbed.cell.1', \
                         'total_sorbed.cell.2', \
                         'total_sorbed.cell.3']
    try:   
        # Amanzi native chemistry
        input_filename = os.path.join("amanzi-u-1d-"+root+".xml")
        path_to_amanzi = "amanzi-native-output"
        run_amanzi_chem.run_amanzi_chem("../"+input_filename,run_path=path_to_amanzi,chemfiles=[root+".bgd"])

        u_amanzi_native = [[[] for x in range(len(amanzi_components))] for x in range(len(times))]
        for i, time in enumerate(times):
           for j, comp in enumerate(amanzi_components):
              x_amanzi_native, c_amanzi_native = GetXY_Amanzi(path_to_amanzi,root,time,comp)
              u_amanzi_native[i][j] = c_amanzi_native

        v_amanzi_native = [[[] for x in range(len(amanzi_sorbed))] for x in range(len(times))]
        for i, time in enumerate(times):
           for j, comp in enumerate(amanzi_sorbed):
              x_amanzi_native, c_amanzi_native = GetXY_Amanzi(path_to_amanzi,root,time,comp)
              v_amanzi_native[i][j] = c_amanzi_native

    except:     

        pass

    try:

        # Amanzi-Alquimia
        input_filename = os.path.join("amanzi-u-1d-"+root+"-alq.xml")
        path_to_amanzi = "amanzi-alquimia-output"
        run_amanzi_chem.run_amanzi_chem("../"+input_filename,run_path=path_to_amanzi,chemfiles=["1d-"+root+".in",root+".dat"])

        u_amanzi_alquimia = [[[] for x in range(len(amanzi_components))] for x in range(len(times))]
        for i, time in enumerate(times):
           for j, comp in enumerate(amanzi_components):
              x_amanzi_alquimia, c_amanzi_alquimia = GetXY_Amanzi(path_to_amanzi,root,time,comp)
              u_amanzi_alquimia[i][j] = c_amanzi_alquimia
              
        v_amanzi_alquimia = [[[] for x in range(len(amanzi_sorbed))] for x in range(len(times))]
        for i, time in enumerate(times):
           for j, comp in enumerate(amanzi_sorbed):
              x_amanzi_alquimia, c_amanzi_alquimia = GetXY_Amanzi(path_to_amanzi,root,time,comp)
              v_amanzi_alquimia[i][j] = c_amanzi_alquimia

        alq = True

    except: 

        alq = False


        # subplots
        #fig, ax = plt.subplots(2,sharex=True,figsize=(15,8))

    colors= ['r','b','y','g'] # components
    styles = ['-','--','x'] # codes
    codes = ['Amanzi+Alquimia(PFloTran)','Amanzi Native Chemistry','PFloTran'] + [None,]*9

    # lines on axes
    # for i, time in enumerate(times):
    i = 1
    for j, comp in enumerate(components):
        if alq:
               ax[0].plot(x_amanzi_alquimia, u_amanzi_alquimia[i][j],color=colors[j],linestyle=styles[0],linewidth=2)
        ax[0].plot(x_amanzi_native, u_amanzi_native[i][j],color=colors[j],linestyle=styles[1],linewidth=2,label=comp)
        ax[0].plot(x_pflotran, u_pflotran[i][j],color=colors[j],linestyle='None',marker=styles[2],linewidth=2)

    # for i, time in enumerate(times):
    i = 1
    for j, comp in enumerate(components):
        if alq:
               ax[1].plot(x_amanzi_alquimia, v_amanzi_alquimia[i][j],color=colors[j],linestyle=styles[0],linewidth=2,label=codes[j*len(styles)])
        ax[1].plot(x_amanzi_native, v_amanzi_native[i][j],color=colors[j],linestyle=styles[1],linewidth=2,label=codes[j*len(styles)+1])
        ax[1].plot(x_pflotran, v_pflotran[i][j],color=colors[j],linestyle='None',marker=styles[2],linewidth=2,label=codes[j*len(styles)+2])

    # axes
    ax[1].set_xlabel("Distance (m)",fontsize=15)
    ax[0].set_ylabel("Total Concentration [mol/L]",fontsize=15)
    ax[1].set_ylabel("Total Sorbed Concent. [mol/m3]",fontsize=15)

    # plot adjustments
    plt.subplots_adjust(left=0.15,bottom=0.15,right=0.99,top=0.90)
    ax[0].legend(loc='upper left',fontsize=15)
    ax[1].legend(loc='lower right',fontsize=15)
    plt.suptitle("Amanzi 1D "+root.title()+" Benchmark at 50 years",x=0.57,fontsize=20)
    plt.tick_params(axis='both', which='major', labelsize=15)

        #pyplot.show()
        #plt.savefig(root+"_1d.png",format="png")
        #plt.close()

    # finally:
    #    pass 
