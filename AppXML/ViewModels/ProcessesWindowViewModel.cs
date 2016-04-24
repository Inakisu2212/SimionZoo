﻿using Caliburn.Micro;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows;

namespace AppXML.ViewModels
{
    public class ProcessesWindowViewModel: Screen
    {
        

        public ProcessManagerViewModel Manager { get; set; }
        public bool isOver { get; set; }


       
        public ProcessesWindowViewModel(List<ProcessStateViewModel> processes)
        {
            Manager = new ProcessManagerViewModel(processes);
            isOver = false;
           

        }

        public ProcessesWindowViewModel(ProcessManagerViewModel pwvm)
        {
            // TODO: Complete member initialization
            Manager = pwvm;
            isOver = false;
           
           
        }

       
        public void Close()
        {
            if(Exit==false)
                Manager.closeAll();
        }
       
        public void CloseWindow()
        {
            Manager.closeAll();
            Exit = true;
            TryClose();
            
                
        }
        public void addProcess(ProcessStateViewModel process)
        {
            Manager.addProcess(process);
        }
        public void addProcess(List<ProcessStateViewModel> processes)
        {
            Manager.addProcess(processes);
        }

        public bool Exit { get; set; }
    }
}
