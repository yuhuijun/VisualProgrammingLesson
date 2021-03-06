//---------------------------------------------------------------------------

#include <vcl.h>
#pragma hdrstop

#include "MultiFtpDownloadThread.h"
#pragma package(smart_init)
#include "MultiFtp.h"
//---------------------------------------------------------------------------

//   Important: Methods and properties of objects in VCL can only be
//   used in a method called using Synchronize, for example:
//
//      Synchronize(UpdateCaption);
//
//   where UpdateCaption could look like:
//
//      void __fastcall MultiFtpDownloadThread::UpdateCaption()
//      {
//        Form1->Caption = "Updated in a thread";
//      }
//---------------------------------------------------------------------------

__fastcall MultiFtpDownloadThread::MultiFtpDownloadThread(bool CreateSuspended)
        : TThread(CreateSuspended)
{
}
__fastcall MultiFtpDownloadThread::~MultiFtpDownloadThread()
{
        closesocket(this->dataClient);
        closesocket(this->commandClient);
}
//---------------------------------------------------------------------------
void __fastcall MultiFtpDownloadThread::Execute()
{
        //---- Place thread code here ----
       TMultiFtp  * multiFtp = (TMultiFtp *)parent;
       SetFilePos(multiFtp->inforImpl.fromToImpl[this->ID].from);
       if(!CreateDataCon())
       {
          multiFtp->runningThreadCnt --;
          return;
       }
       String str = "RETR "+this->FileName+" \r\n";
       char *buffer = new char[100];
       int recLen ;
       send(commandClient,str.c_str(),str.Length(),0);
       this->DoOnTextOut(str.SubString(1,str.Length()-3));
       recLen = recv(commandClient,buffer,100,0);
       this->DoOnTextOut(buffer);
        buffer[recLen]=0;
       if(multiFtp->GetCode(buffer) != "150")
       {
          Sleep(1000);
          send(commandClient,str.c_str(),str.Length(),0);
          this->DoOnTextOut("尝试第2次！");
          recv(commandClient,buffer,100,0);
          this->DoOnTextOut(buffer);
          if(multiFtp->GetCode(buffer) != "150")
          {
            Sleep(1000);
            send(commandClient,str.c_str(),str.Length(),0);
             this->DoOnTextOut("尝试第3次！");
            recv(commandClient,buffer,100,0);
            this->DoOnTextOut(buffer);
            if(multiFtp->GetCode(buffer) != "150")
            {
                this->DoOnException(buffer);
                delete[] buffer;
                multiFtp->runningThreadCnt --;
                return;
            }
          }
       }
          this->DoOnTextOut("开始接受数据！");
        while(!multiFtp->stop && multiFtp->inforImpl.fromToImpl[this->ID].from < multiFtp->inforImpl.fromToImpl[this->ID].to)
        {
             if(multiFtp->inforImpl.fromToImpl[this->ID].to - multiFtp->inforImpl.fromToImpl[this->ID].from > (DWORD)multiFtp->PerGetLen)
               {
                 if(!DownLoad(multiFtp->inforImpl.fromToImpl[this->ID].from,multiFtp->PerGetLen))
                   break;
               }
             else
                {
                if(!DownLoad(multiFtp->inforImpl.fromToImpl[this->ID].from,multiFtp->inforImpl.fromToImpl[this->ID].to - multiFtp->inforImpl.fromToImpl[this->ID].from))
                    break;
                }
        }
        delete[] buffer;
        closesocket(this->dataClient);
        closesocket(this->commandClient);
        multiFtp->runningThreadCnt --;
}
bool __fastcall MultiFtpDownloadThread::DownLoad(DWORD pos ,DWORD len)
{
    TMultiFtp  * multiFtp = (TMultiFtp *)parent;
     char *buffer  = new char[len];
     DWORD cnt =0 ;
     while(!multiFtp->stop && cnt < len)
     {
       int recLen = recv(this->dataClient,buffer+cnt,len-cnt,0);
       if(recLen > 0)
        {
           cnt += recLen;
           multiFtp->FilePos += recLen;
           multiFtp->inforImpl.fromToImpl[this->ID].from += recLen;
           this->DoOnProgress(multiFtp->FilePos);
        }
        else if(recLen < 0)
        {
            this->DoOnException("下载有误");
            break;
        }
        else
        {
          continue;
        }
     }
    if(cnt < len || cnt == 0)
       {
        this->DoOnException("下载文件失败！");
        delete[] buffer;
        return false;
       }
     else
     {
      //  this->DoOnTextOut("下载了一部分数据");
        multiFtp->WriteToFile(this->localFileLoad,pos,buffer,len);
        multiFtp->WriteInforToFile();
       if(multiFtp->FilePos == multiFtp->fileSize)
       {
           this->DoOnComplete();
           multiFtp->WriteInforToFile();
           String profile = multiFtp->FileName+".san";
           fclose(multiFtp->globalFile);
           String oldName = multiFtp->LocalLoad + ".nam";
           rename(oldName.c_str(),multiFtp->LocalLoad.c_str());
           DeleteFile(profile);
        }
     }
     delete[] buffer;
     return true;
}
bool __fastcall MultiFtpDownloadThread::SetFilePos(DWORD pos)
{
   TMultiFtp  * multiFtp = (TMultiFtp *)parent;
   int recLen ;
   char * buffer = new char[100];
   String str = "REST "+IntToStr(pos)+" \r\n";
   this->DoOnTextOut(str);
   send(this->commandClient,str.c_str(),str.Length(),0);
   recLen = recv(commandClient,buffer,100,0);
   buffer[recLen]=0;
   if(multiFtp->GetCode(buffer) != "350")
   {
     this->DoOnException(buffer);
     delete[] buffer;
     return false;
   }
   this->DoOnTextOut(buffer);
   delete[] buffer;
   return true;
}
bool __fastcall MultiFtpDownloadThread::CreateDataCon()
{
    char *port0= "PASV \r\n" ;
    TMultiFtp  * multiFtp = (TMultiFtp *)parent;
    this->DoOnTextOut(port0);
    char *buffer = new char[100];
    int recLen ;
    send(this->commandClient,port0,strlen(port0),0);
    recLen = recv(commandClient,buffer,100,0);
     buffer[recLen]=0;
    if(multiFtp->GetCode(buffer) == "227")
    {
      this->DoOnTextOut(buffer);
    }
    else
    {
       this->DoOnException(buffer);
       delete[] buffer;
       return false;
    }
    MultiThreadDealSocket *dealSocket = new MultiThreadDealSocket();
    String host1 = dealSocket->GetHost(buffer);
    int port1 = dealSocket->GetPort(buffer);
    delete[] buffer;
    this->dataClient = dealSocket->GetConnect(host1,port1);
    if(this->dataClient != NULL)
    {
       this->DoOnTextOut("连接指定的断口成功！" );
      return true;
    }
    else
    {
       this->DoOnException("连接失败！！！");
       return false;
    }
}
//---------------------------------------------------------------------------
void __fastcall MultiFtpDownloadThread::DoOnComplete()
{
  if(this->FOnComplete)
     this->FOnComplete(this->Owner);
}
void __fastcall MultiFtpDownloadThread::DoOnTextOut(String text)
{
  if(this->FOnTextOut)
  {
     int index = text.Pos("\r\n");
     if(index > 0)
       this->FOnTextOut(this->Owner,text.SubString(1,index-1));
     else
       this->FOnTextOut(this->Owner,text);
  }
}
void __fastcall MultiFtpDownloadThread::DoOnException(String error)
{
   if(this->FOnException)
   {
      int index = error.Pos("\r\n");
      if(index > 0)
      {
        this->FOnException(this->Owner,error.SubString(1,index-1));
      }
      else
      {
          this->FOnException(this->Owner,error);
      }
   }
}
void __fastcall MultiFtpDownloadThread::DoOnProgress(DWORD pos)
{
  if(this->FOnProgress)
    this->FOnProgress(this->Owner,pos);
}
