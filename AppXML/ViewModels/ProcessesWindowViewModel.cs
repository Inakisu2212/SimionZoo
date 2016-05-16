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


        public ProcessManagerViewModel Manager { get { return _manager; } set { _manager = value; NotifyOfPropertyChange(()=>Manager);}  }
        private ProcessManagerViewModel _manager;
        public bool isOver { get { return _isOver; } set { _isOver = value;NotifyOfPropertyChange(()=>isOver); } }
        private int numOfProcesses;
        private bool _isOver;
        public void isFinished(int count)
        {
            numOfProcesses -= count;
            if (numOfProcesses == 0)
                isOver = true;
        }
        public ProcessesWindowViewModel()
        {
            _isOver = false;
        }
        public ProcessesWindowViewModel(List<ProcessStateViewModel> processes)
        {
            Manager = new ProcessManagerViewModel(processes);
            numOfProcesses = processes.Count;
            isOver = false;
           

        }

        public ProcessesWindowViewModel(ProcessManagerViewModel pwvm)
        {
            // TODO: Complete member initialization
            Manager = pwvm;
            numOfProcesses = pwvm.Processes.Count;
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
