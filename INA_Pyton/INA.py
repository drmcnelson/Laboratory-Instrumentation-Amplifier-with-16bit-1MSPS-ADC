#!/usr/bin/python

"""
MCP33131D.py

Mitchell C. Nelson (c) 2024

"""

__author__    = "Mitchell C. Nelson, PhD"
__copyright__ = "Copyright 2024, Mitchell C, Nelson"
__version__   = "0.1"
__email__     = "drmcnelson@gmail.com"
__status__    = "alpha testing"

import sys
import time
import select

import os
import io
import subprocess

import operator

import signal
import atexit

#from timeit import default_timer as timer

import platform

import serial

from datetime import datetime
from time import sleep, time, process_time, thread_time

# from threading import Lock, Semaphore, Thread
#from queue import SimpleQueue, Empty
from multiprocessing import Process, Queue, Value, Lock
import queue

import struct

import inspect
from itertools import count

import numpy as np

import matplotlib.pyplot as plt

plt.rcParams['toolbar'] = 'toolmanager'
from matplotlib.backend_tools import ToolBase, ToolToggleBase

from matplotlib.figure import Figure
from matplotlib.animation import FuncAnimation
from matplotlib.axis import Tick

from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
from matplotlib.backends.backend_tkagg import NavigationToolbar2Tk


versionstring = os.path.basename(__file__) + ' - version %s %s'%(__version__,__copyright__)

def input_ready():
    return (sys.stdin in select.select([sys.stdin], [], [], 0)[0])


def dumpdictionary( dictionary, exclude=None, file=sys.stdout ):

    for key,val in dictionary.items():

        if exclude is not None:
            if key in exclude:
                continue

        if type(val) in [ float, int ] :
            file.write( '# ' + key + ' = ' + str(val) + '\n' )

        elif type(val) in [ str ] :
            file.write( '# ' + key + ' = "' + str(val) + '"\n' )

        elif type(val) in [ list ] :
            if not len(val):
                file.write( '# ' + key + ' = []\n' )
            else:
                values = val
                if type(values[0]) in [ float, int ] :
                    file.write( '# ' + key + ' = [ ' + ', '.join( [ str(v) for v in values ] ) + ' ]\n'  )
                elif type(values[0]) in [ str ] :
                    file.write( '# ' + key + ' = [ "' + '", "'.join( [ str(v) for v in values ] ) + '" ]\n'  )

def stringSubstitution( s, mydict ):
        
    try:
        exec( "newname__=\""+s+"\"", mydict, globals() )
        print(newname__)
    except Exception as e:
        print( e )
        return None

    return newname__

def findSubstring(s, key):
    if key in s:
        try:
            idx = s.index(key)
            return s[idx:]
        except Exception as e:
            print( s, key, e )
    return None

# ========================================================================                    

class MCP33131D:

    _ids = count(0)


    def __init__( self, portspec, readtimeout=1., writetimeout=1., graphics=True, debug=False ):

        self.versionstring = os.path.basename(__file__) + ' - version %s %s'%(__version__,__copyright__)
        
        self.ser = serial.Serial( portspec, timeout=readtimeout, write_timeout=writetimeout )

        self.instance = next( self._ids )
        
        self.name= portspec

        self.responsequeue = Queue()
        self.dataqueue = Queue()
        self.graphicsqueue = Queue()

        self.flag = Value( 'i', 1 )
        self.errorflag = Value( 'i', 0 )

        # These are the records from "Setup:" until data is queued to the graphics thread
        self.setuprecords = []        

        if debug:
            self.debug = Value( 'i', 1 )
        else:
            self.debug = Value( 'i', 0 )

        self.filesuffix = ".data"

        self.readerthread = Process( target = self.reader )
        self.readerthread.start()

        self.graphicsthread = Process( target = self.graphicsWindow )
        self.graphicsthread.start()

        self.graphics_history = []
        self.graphics_history_pointer = 0

        
        sleep(1)
        self.commandwrite("version")
        self.commandwrite("configuration")
        

    def setDebug(self,debug=True):
        if debug:
            self.debug.value = 1
        else:
            self.debug.value = 0

    def clearDebug(self):
        self.debug.value = 0

    def close( self, ignored=None ):

        self.flag.value = 0
        sleep( 0.1 )

        if self.readerthread is not None:
            self.readerthread.terminate()
            self.readerthread.join()

        if self.graphicsthread is not None:
            self.graphicsthread.terminate()
            self.graphicsthread.join()
            
    def write(self,s):
        self.ser.write( s.encode() )

    # ========================================================        
    def commandwrite(self,s,timeout=1):
        responses = self.clearResponseQueue()
        if responses and len(responses):
            for response in responses:
                if not type(response) is list:
                    response = [response]
                for r in response:
                    if r.startswith("Setup:"):
                        try:
                            r = r.split(maxsplit=1)[1]
                            self.setuprecords = [r]
                        except Exception as e:
                            print("response r", e)
                            self.setuprecords = []
                    else:
                        self.setuprecords.append(r)

        with self.errorflag.get_lock():
            self.errorflag.value = 0

        print("Command:", s)
        self.write(s+'\n')

        # First response we wait for timeout
        try:
            responses = self.responsequeue.get(True,timeout)
            for response in responses:
                if not type(response) is list:
                    response = [response]
                for r in response:
                    print("response:",r)                
                    if r.startswith("Setup:"):
                        try:
                            r = r.split(maxsplit=1)[1]
                            self.setuprecords = [r]
                        except Exception as e:
                            print("response r", e)
                            self.setuprecords = []
                    else:
                        self.setuprecords.append(r)
                    
        except Exception as e:
            print(e)

        return (self.errorflag.value == 0)

    # -------------------------------------------------------
    def clearResponseQueue(self,timeout=10):
        responses = []
        try:
            while not self.responsequeue.empty():
                response = self.responsequeue.get(True,timeout)
                responses.append(response)
        except Exception as e:
            print(e)
        return responses
        
    def clearDataQueue(self):
        print( "clearDataQueue")
        records = []
        while not self.dataqueue.empty():
            record = self.dataqueue.get(False)
            records.append(record)
        return records
    
    # ----------------------------------------------------------            
    def reader( self ):

        data_put = self.dataqueue.put
        graph_put = self.graphicsqueue.put
        response_put = self.responsequeue.put

        read_until = self.ser.read_until
        
        responselist = []

        dlist = []
        tlist = []

        volts_per_lsb = None
        is_signed = False

        is_clocked_singles = False
        is_clocked_buffer = False
        clock_usecs = None
        singles_length = None
        buffer_length = None
        update_length = None
        
        while self.flag.value:
            
            try:
                buffer = read_until( )
                buffer = buffer.decode()[:-1]
            except Exception as e:
                print("reader error", e)
                continue
            
            if buffer is None:
                continue

            if not len(buffer):
                continue

            if self.debug.value:
                print("reader:", buffer)
            
            if buffer[0] == '#':
                if not len(responselist):
                    print(buffer)
                
            elif buffer[:4] == 'Res:':
                if not len(responselist):
                    print(buffer)

            # Receive Binary Formatted Data Buffer
            elif buffer.startswith( "BINARY16" ):
                ndata = int(buffer[8:])

                if self.debug.value:
                    print("has binary data", ndata)

                # Read the data
                data = self.ser.read( ndata*2 )

                timestamp = datetime.now()

                # Read(Expect) the end of data message
                endbuffer = self.ser.read_until( )
                endbuffer = endbuffer.decode()[:-1]

                if self.debug.value:
                    print( "endbuffer:", endbuffer)
                
                if endbuffer.startswith( "END" ):

                    if is_signed:
                        data = struct.unpack( '<%dh'%(len(data)/2), data )
                    else:
                        data = struct.unpack( '<%dH'%(len(data)/2), data )
                        
                    if self.debug.value:
                        print("received binary data:", data)
                        
                    if volts_per_lsb:
                        data = np.array(data) * volts_per_lsb
                        if self.debug.value:
                            print("scaled data:", data)

                    if buffer_length is not None and clock_usecs is not None and len(data) == buffer_length:
                        dt = clock_usecs * 1.E-6
                        tstop = (buffer_length-1) * dt
                        t = np.linspace(0,tstop,buffer_length)
                        self.dataqueue.put([timestamp,[t,data]])
                        graph_put([timestamp,[t,data]])
                        if self.debug.value:
                            print( "put t,d", timestamp, t, data)
                            print(datetime.now().strftime('%Y%m%d.%H%M%S.%f'))
                    else:
                        self.dataqueue.put([timestamp,[data]])
                        graph_put([timestamp,[data]])
                        if self.debug.value:
                            print( "put data", timestamp, data)
                            print(datetime.now().strftime('%Y%m%d.%H%M%S.%f'))

                        
            
            elif buffer.startswith( "DATA_INTS" ):
                ndata = int(buffer[9:])

                data = []
                for n in range(ndata):
                    try:
                        dbuf = read_until( )
                        dbuf = dbuf.decode()[:-1]
                        idata = int(dbuf)
                        data.append(idata)
                    except Exception as e:
                        print("reading data_ints", e)
                        continue
                    
                timestamp = datetime.now()

                endbuffer = self.ser.read_until( )
                endbuffer = endbuffer.decode()[:-1]

                if self.debug.value:
                    print( "endbuffer:", endbuffer)

                if endbuffer.startswith( "END" ):
                    if volts_per_lsb:
                        data = np.array(data) * volts_per_lsb
                        if self.debug.value:
                            print("scaled data:", data)

                    #if buffer_length is not None and clock_usecs is not None and len(data) == buffer_length:
                    if is_clocked_buffer and buffer_length and clock_usecs and len(data) == buffer_length:
                        dt = clock_usecs * 1.E-6
                        tstop = (buffer_length-1) * dt
                        t = np.linspace(0,tstop,buffer_length)
                        self.dataqueue.put([timestamp,[t,data]])
                        graph_put([timestamp,[t,data]])
                        if self.debug.value:
                            print( "put", timestamp, t, data)
                    else:
                        self.dataqueue.put([timestamp,[data]])
                        graph_put([timestamp,[data]])
                        if self.debug.value:
                            print( "put", timestamp, data)
                    

            elif buffer.startswith( "DATA_VOLTS" ):
                ndata = int(buffer[10:])

                data = []
                for n in range(ndata):
                    try:
                        dbuf = read_until( )
                        dbuf = dbuf.decode()[:-1]
                        fdata = float(dbuf)
                        data.append(fdata)
                    except Exception as e:
                        print("reading data_ints", e)
                        continue
                    
                timestamp = datetime.now()

                endbuffer = self.ser.read_until( )
                endbuffer = endbuffer.decode()[:-1]

                if self.debug.value:
                    print( "endbuffer:", endbuffer)
                    
                if endbuffer.startswith( "END" ):

                    #if buffer_length is not None and clock_usecs is not None and len(data) == buffer_length:
                    if is_clocked_buffer and buffer_length and clock_usecs and len(data) == buffer_length:
                        dt = clock_usecs * 1.E-6
                        tstop = (buffer_length-1) * dt
                        t = np.linspace(0,tstop,buffer_length)
                        self.dataqueue.put([timestamp,[t,data]])
                        graph_put([timestamp,[t,data]])
                        if self.debug.value:
                            print( "put", timestamp, t, data)
                    else:
                        self.dataqueue.put([timestamp,[data]])
                        graph_put([timestamp,[data]])
                        if self.debug.value:
                            print( "put", timestamp, data)
                
            elif buffer.startswith("START"):
                dlist = []
                tlist = []
                timestamp = datetime.now()

            elif buffer.startswith("adc:"):
                pars = buffer.split()
                try:
                    t = float(pars[1])
                    d = int(pars[2])
                    tlist.append(t)
                    dlist.append(d)
                    
                    if update_length and not (len(dlist)%update_length):
                        graph_put([datetime.now(),[tlist,dlist]])
                        
                except Exception as e:
                    print(e)
                    
            elif buffer.startswith("ADC:"):
                pars = buffer.split()
                try:
                    t = float(pars[1])
                    d = float(pars[2])
                    tlist.append(t)
                    dlist.append(d)

                    if update_length and not (len(dlist)%update_length):
                        graph_put([datetime.now(),[tlist,dlist]])
                    
                except Exception as e:
                    print(e)
                    continue

            elif buffer.startswith("END"):
                if len(tlist) and len(dlist):
                    self.dataqueue.put([timestamp,[tlist,dlist]])
                    graph_put([timestamp,[tlist,dlist]])
                    if self.debug.value:
                        print( "put", timestamp, tlist, dlist)
                        
            elif buffer.startswith("Configuration"):
                pars = buffer.split()

                try:
                    if pars.index("signed"):
                        is_signed = True
                except Exception as e:
                    pass
                
                try:
                    idx = pars.index("volts_per_lsb")
                    volts_per_lsb = float(pars[idx+1])
                except Exception as e:
                    print(e)

            elif buffer.startswith("Setup:"):                
                s = buffer[6:].strip()
                is_clocked_single = s.startswith("ClockedSingle")
                is_clocked_buffer = s.startswith("ClockedBuffer")
                clock_usecs = None
                buffer_length = None
                singles_length = None
                
            elif buffer.startswith("Clock:"):
                try:
                    clock_usecs = int(buffer.split()[-1])
                    update_length = 100000/clock_usecs
                            
                except Exception as e:
                    print(buffer, e)
                    
            elif buffer.startswith("Buffer:"):
                try:
                    buffer_length = int(buffer.split()[6])
                except Exception as e:
                    print(buffer, e)
            
            elif buffer.startswith("Singles:"):
                try:
                    singles_length = int(buffer.split()[1])
                except Exception as e:
                    print(buffer,e)
                        
            elif buffer.startswith("Error:"):
                with self.errorflag.get_lock():
                    self.errorflag.value += 1

            responselist.append(buffer)            
            if buffer[:4] == "DONE":
                if self.debug.value:
                    print("===============================")
                    print("posting to queue")
                    print(responselist)
                    print("===============================")
                response_put(responselist)
                responselist = []
                                
    # ========================================================        
    def saveDataFile(self, file, records=None, timestamp=None, comment=None):        
        if timestamp is None:
            timestamp = datetime.now()

        # ------------------------------------------------            
        if type(file) is str:

            print('saveDataFile file', file)
            print('saveDataFile records', records)
            print('saveDataFile timestamp', timestamp)
            print('saveDataFile comment', comment)
            print('saveDataFile records', records)
            
            file = stringSubstitution(file,self.__dict__)

            print("saveDataFile =>", file )
            
            if not file.endswith( self.filesuffix ):
                file += "." + timestamp.strftime('%Y%m%d.%H%M%S.%f') + self.filesuffix

            print( 'saveto', file )

            # Existing directory, or, create it
            dirspec = os.path.dirname(file)
            if len(dirspec):
                if os.path.isdir(dirspec):
                    print( 'saving to directory', dirspec )
                else:
                    print( 'creating directory', dirspec )
                    try:
                        os.makedirs(dirspec)
                    except Exception as e:
                        print( e )
                        return False

            # Open new file for write, allow new file only
            try:
                file = open(file,"x")
            except Exception as e:
                print( "open ", file )
                print( e )
                return None

            print( 'fileopened', file )
            retv = self.saveDataFile(file, records, timestamp, comment)

            file.close()

            return retv

        # ==========================================================================
        if not isinstance(file, io.IOBase):
            print( "need to call saveData with filename or open filehandle")
            return None
        
        # ------------------------------------------------
        if records is None:
            records = []
            try:
                r = self.dataqueue.get(True,5.0)
                records = [r]
            except Exception as e:
                print("no records in 5 seconds")
                #print(datetime.now().strftime('%Y%m%d.%H%M%S.%f'))
                return None

            while True:
                try:
                    r = self.dataqueue.get(True,0.1)
                    records.append(r)                    
                except Exception as e:
                    print("got", len(records), e)
                    break
                

        if records is None:
            nrecords = 0
        else:
            nrecords = len(records)
            
        print("saveDataFile", file, nrecords, timestamp, comment )
        # ------------------------------------------------

        
        print(file)
        
        file.write( '# %s\n'%(self.versionstring) )
        file.write( '# ' + timestamp.strftime('%Y-%m-%d %H:%M:%S.%f') + '\n' )

        dumpdictionary( self.__dict__,  exclude=['filesuffix','setuprecords'], file=file )

        if len(self.setuprecords):
            for r in self.setuprecords:
                if not r.startswith("DONE"):
                    file.write( "# setup: " + r + '\n')
        
        if comment is not None:
            file.write( '# comment='+comment+'\n' )
        
        file.write( '# header end\n' )

        # ------------------------------------------------                    
        print( 'writing', len(records), 'records' )
            
        for nrec,record in enumerate(records):

            if record is None or not len(record):
                print("null record", nrec)

            elif type(record) is str:
                file.write( '# ' + record.strip() + '\n' )
   
            else:
                timestamp,data = record
                
                file.write( '# TIMESTAMP: '+timestamp.strftime('%Y-%m-%d %H:%M:%S.%f')+'\n' )
                
                if len(data) == 2:
                    
                    xcol,ycol = data
                    file.write( "# DATA ASCII %d\n"%(len(ycol)) )
                    for x,y in zip(xcol,ycol):
                        file.write("%.8f %.8f\n"%(x,y))
                    file.write( "# END DATA\n" )
                    
                elif len(data) == 1:
                    ycol = data[0]
                    file.write( "# DATA ASCII %d\n"%(len(ycol)) )
                    for y in ycol:
                        file.write("%.8f\n"%(y))
                    file.write( "# END DATA\n" )                                

                if nrec+1 < len(records):
                    file.write( "\n" )
                    file.write( "\n" )

        return file

    # ================================================================================
    def graphRecord_(self,record):

        if record is None:
            return

        if len(record) == 0:
            return
        
        if type(record) is str:
            return
        
        timestamp,data = record
        self.graphics_text.set_text( timestamp.strftime('%Y-%m-%d %H:%M:%S.%f') )

        
        if len(data) == 2:
            self.graphics_line.set_data(data[0],data[1])
            self.graphics_x = data[0]
            self.graphics_y = data[1]
                        
        elif len(data) == 1:
            self.graphics_y = data[0]
            if len(self.graphics_y) == len(self.graphics_x):
                self.graphics_line.set_ydata(self.graphics_y)
            else:
                ndata = len(self.graphics_y)
                self.graphics_x = np.linspace(0,ndata-1,ndata)
                self.graphics_line.set_data(self.graphics_x,self.graphics_y)

        xwidth = np.abs(self.graphics_x[-1] - self.graphics_x[0])
        xmin = np.min(self.graphics_x) - xwidth/20.
        xmax = np.max(self.graphics_x) + xwidth/20.
        plt.xlim(xmin,xmax)

        ywidth = np.abs(self.graphics_y[-1] - self.graphics_y[0])
        ymin = np.min(self.graphics_y) - ywidth/20.
        ymax = np.max(self.graphics_y) + ywidth/20.
        plt.ylim(ymin,ymax)
        
        return
    
    def graphicsUpdate(self, dummy):

        if self.flag.value:
            while True:
                try:
                    record = self.graphicsqueue.get(block=False)

                    self.graphRecord_(record)

                    self.graphics_history.append(record)
                    
                    if len(self.graphics_history) > 100:
                        self.graphics_history.pop(0)
                    self.graphics_history_pointer = len(self.graphics_history)-1
                        
                except queue.Empty:
                    break

        else:
            #self.ani.event_source.stop()
            try:
                plt.close()
            except Exception as e:
                print("animation plt.close()", e )

        return [self.graphics_text, self.graphics_line, self.graphics_ax]
            
        
    class LastGraph(ToolBase):
        default_keymap = 'up'
        description = 'last data vector'

        def __init__(self, *args, **kwargs):
            self.graph = kwargs.pop('graphobj')
            super().__init__(*args, **kwargs)
            #ToolBase.__init__(self, *args, **kwargs)
            
        def trigger(self, *args, **kwargs):
            #print('last graph key pressed')

            try:
                record = self.graph.graphics_history[-1]
                self.graph.graphics_history_pointer = len(self.graph.graphics_history)-1

                self.graph.graphRecord_( record )
                self.graph.graphics_fig.canvas.draw()
            except Exception as e:
                print("LastGraph", e )
            

    class NextGraph(ToolBase):
        default_keymap = 'right'
        description = 'next data vector'

        def __init__(self, *args, **kwargs):
            self.graph = kwargs.pop('graphobj')
            super().__init__(*args, **kwargs)
            #ToolBase.__init__(self, *args, **kwargs)
            
        def trigger(self, *args, **kwargs):
            #print('next graph key pressed')

            if self.graph.graphics_history_pointer < len(self.graph.graphics_history) - 1:

                self.graph.graphics_history_pointer += 1        
                record = self.graph.graphics_history[self.graph.graphics_history_pointer]
                
                self.graph.graphRecord_( record )
                self.graph.graphics_fig.canvas.draw()
            else:
                print( 'already at last graph' )

    class PreviousGraph(ToolBase):
        default_keymap = 'left'
        description = 'prev data vector'

        def __init__(self, *args, **kwargs):
            self.graph = kwargs.pop('graphobj')
            super().__init__(*args, **kwargs)
            #ToolBase.__init__(self, *args, **kwargs)
            
        
        def trigger(self, *args, **kwargs):
            #print('previous graph key pressed')
            
            if self.graph.graphics_history_pointer > 0:
                self.graph.graphics_history_pointer -= 1        
                record = self.graph.graphics_history[self.graph.graphics_history_pointer]                
                self.graph.graphRecord_( record )
                self.graph.graphics_fig.canvas.draw()
            else:
                print( 'already at first graph' )

                
    def graphicsWindow(self, npoints=128, ncurves=1, geometry=None, blit=False):

        print( 'top of graphics')
        
        #self.graphics_fig, self.graphics_ax = plt.subplots()

        self.graphics_fig = plt.figure(self.name)
        
        self.graphics_history = []

        self.graphics_history_pointer = 0
        
        if geometry is not None:
            dpi = self.graphics_fig.get_dpi()
            width,height = geometry.lower().split('x',maxsplit=1)
            self.graphics_fig.set_size_inches( int(width)/dpi, int(height)/dpi )
            
        self.graphics_fig.subplots_adjust(top=0.9)
        
        # This gives us scrolling through the history record
        self.graphics_fig.canvas.manager.toolmanager.add_tool('Prev', self.PreviousGraph, graphobj=self )
        self.graphics_fig.canvas.manager.toolmanager.add_tool('Next', self.NextGraph, graphobj=self )
        self.graphics_fig.canvas.manager.toolmanager.add_tool('Last', self.LastGraph, graphobj=self )
        self.graphics_fig.canvas.manager.toolbar.add_tool('Prev', 'toolgroup', -1)
        self.graphics_fig.canvas.manager.toolbar.add_tool('Next', 'toolgroup', -1)
        self.graphics_fig.canvas.manager.toolbar.add_tool('Last', 'toolgroup', -1)
                
        self.graphics_ax = self.graphics_fig.add_subplot(1, 1, 1)

        
        self.graphics_x = np.linspace(0,2*np.pi,npoints)
        self.graphics_y = np.sin(self.graphics_x)

        self.graphics_line, = self.graphics_ax.plot(self.graphics_y)

        self.graphics_text = self.graphics_ax.text(0.99, 0.99, '0',
                                          horizontalalignment='right',
                                          verticalalignment='top',
                                          transform=self.graphics_ax.transAxes)
        
        self.ani = FuncAnimation(self.graphics_fig, self.graphicsUpdate, interval=200, blit=blit )

        plt.show()

        try:
            plt.close()
        except Exception as e:
            print("graphicsWindow plt.close()", e )

        self.flag.value=0
        
        self.graphics_fig = None
        
    # ================================================================================
    def cli_(self,line):

        if '%' in line and not '=' in line:
            try:
                exec( 'temporary_variable_='+line, self.__dict__, globals())            
                line = temporary_variable_
                print(line)
            except Exception as e:
                print(e)
                return False

        # ----------------------------------------
        # Dump the namespaces
        if line == 'dump all':
            for key, val in globals().items():
                print( "global ", key, val )
            for key, val in locals().items():
                print( "local ",key, val )
            for key, val in self.__dict__.items():
                print( "__dict__ ",key, val )
            return True

        # ----------------------------------------
        # Dump variables from the class dictionary
        if line == '=':
            dumpdictionary(self.__dict__)
            dumpdictionary(globals())
            return True

        # ----------------------------------------
        # Execute in python command processor
        if line.startswith( '=' ):
            try:
                exec( line[1:].strip(), self.__dict__, globals() )
                return True
            except Exception as e:
                print( e )
                return False

        if '=' in line:
            try:
                print( 'executing line:', line )
                exec( line, self.__dict__, globals() )
                return True
            except Exception as e:
                print( e )
                return False

        # --------------------------------------
        # Shell processing
        if line.startswith('!'):
            try:
                p = subprocess.Popen(line[1:], shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
                #p = subprocess.Popen(line[1:].split(), stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
                output,errors = p.communicate()
                if output:
                    print("Output:")
                    print(output.strip())
                if errors:
                    print("Errors:", errors.strip())
                if p.returncode:
                    print( "returncode", p.returncode, "(failed)")
                    return False
            except Exception as e:
                print(e)
                return False
            return True

        # --------------------------------------
        # Batch/indirect command processing
        if line.startswith('@'):
            try:
                linenumber = 0
                with open(line[1:]) as fp:
                    for line in fp:
                        linenumber += 1
                        line = line.strip()
                        if len(line) :
                            print( "*****")
                            print( "line %d:"%(linenumber), line)
                            if not self.cli_(line):
                                return False
            except Exception as e:
                print(e)
                return False
            return True
        
        # --------------------------------------
        # Save to file
        if line.startswith('save'):
            print('save:', line)
            pars = line.split(maxsplit=2)
            if len(pars) < 2:
                print( "need filepsec: save filespec [comment....]" )
                return False
            print('parse[1]', pars[1])
            retv = self.saveDataFile(pars[1], comment=' '.join(pars[2:]))
            print( 'retv', retv )
            return (retv is not None)

        # --------------------------------------
        # Debug
        if line.startswith("debug"):
            pars = line.split(maxsplit=1)
            if len(pars)<2:
                print( "Debug:", self.debug.value)
                return True

            if pars[1] in ['1','true','on']:
                self.debug.value=1
                print( "debug on")
                return True
            
            if pars[1] in ['0','false','off']:
                self.debug.value=0
                print( "debug off")
                return True

            print( "debug parameter not recognized")
            return False
            
        # --------------------------------------
        # Pass it to the connected device
        return self.commandwrite(line)
        
    # ------------------------------------------------------------------------
    def cli(self):
        
        while self.flag.value:

            try:
                line = input( self.name + ':' )
            except KeyboardInterrupt:
                print('^C')
                line = input( "do you want to exit?" )
                if bool(line):
                    self.flag.value = 0
                    break

            if line.lower() in ['exit', 'quit', 'q' ]:
                self.flag.value = 0
                break

            retv = self.cli_(line)
            print( retv )

        return self.flag.value
            
#=======================================================================================

if __name__ == "__main__":

    import argparse

    try:
        import readline
    except:
        pass

    import atexit
    import signal
    
    if platform.system() == 'Linux':
        ser0_default = '/dev/ttyACM0'
        ser1_default = '/dev/ttyACM1'
    elif platform.system() == 'Windows':
        ser0_default = 'COM1:'
        ser1_default = 'COM2:'

    # ---------------------------------------------------------
    def SignalHandler(signal, frame):

        serialdevice.close()
        sleep(0.1)
            
        print('Exit')
        sys.exit(0)            
    
    # ---------------------------------------------------------
    class ExplicitDefaultsHelpFormatter(argparse.ArgumentDefaultsHelpFormatter):
        def _get_help_string(self, action):
            if action.default in (None, False):
                return action.help
            return super()._get_help_string(action)
    
    parser = argparse.ArgumentParser( description='MCP33131D Controller Monitor/Cli',
                                      formatter_class=ExplicitDefaultsHelpFormatter )

    parser.add_argument( 'ports', default=[ser0_default], nargs='*',
                         help = 'one or more serial or com ports,' +
                         ' the first is the control port, others are readonly' )

    parser.add_argument( '--historyfile', default = 'mcp33131d.history', help='history file for the command line interface' )
    parser.add_argument( '--nohistoryfile' )
        
    args = parser.parse_args()           

    serialdevice = MCP33131D( args.ports[0] )


    if not args.nohistoryfile:
        try:
            readline.read_history_file(args.historyfile)
        except Exception as e:
            print('historyfile: ', e)
            print('continuing')
            
        atexit.register(readline.write_history_file, args.historyfile)

    signal.signal(signal.SIGINT, SignalHandler)
    
    # ---------------------------------------------------------

    serialdevice.cli()

    serialdevice.close()
