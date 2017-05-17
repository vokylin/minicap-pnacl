// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <string.h>
#include <sstream>

#include "ppapi/cpp/host_resolver.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/tcp_socket.h"
#include "ppapi/cpp/udp_socket.h"
#include "ppapi/cpp/var.h"
#include "ppapi/utility/completion_callback_factory.h"
#include "ppapi/c/ppb_image_data.h"
#include "ppapi/cpp/graphics_2d.h"
#include "ppapi/cpp/image_data.h"
#include <ppapi/cpp/var_array_buffer.h>

#include "jpeglib.h"
#include "jerror.h" 

#ifdef WIN32
#undef PostMessage
// Allow 'this' in initializer list
#pragma warning(disable : 4355)
#endif

#define MSG_CREATE_TCP 't'
#define MSG_CREATE_UDP 'u'
#define MSG_SEND 's'
#define MSG_CLOSE 'c'
#define MSG_LISTEN 'l'

static const int kBufferSize = 10485760;

class Banner {
  public: 
  int getVersion() {
    return version;
  }
  void setVersion(int version) {
    this->version = version;
  }
  int getLength() {
    return length;
  }
  void setLength(int length) {
    this->length = length;
  }
  int getPid() {
    return pid;
  }
  void setPid(int pid) {
    this->pid = pid;
  }
  int getReadWidth() {
    return readWidth;
  }
  void setReadWidth(int readWidth) {
    this->readWidth = readWidth;
  }
  int getReadHeight() {
    return readHeight;
  }
  void setReadHeight(int readHeight) {
    this->readHeight = readHeight;
  }
  int getVirtualWidth() {
    return virtualWidth;
  }
  void setVirtualWidth(int virtualWidth) {
    this->virtualWidth = virtualWidth;
  }
  int getVirtualHeight() {
    return virtualHeight;
  }
  void setVirtualHeight(int virtualHeight) {
    this->virtualHeight = virtualHeight;
  }
  int getOrientation() {
    return orientation;
  }
  void setOrientation(int orientation) {
    this->orientation = orientation;
  }
  int getQuirks() {
    return quirks;
  }
  void setQuirks(int quirks) {
    this->quirks = quirks;
  }
  private:
  int version;
  int length;
  int pid;
  int readWidth;
  int readHeight;
  int virtualWidth;
  int virtualHeight;
  int orientation;
  int quirks;
};

class ExampleInstance : public pp::Instance {
 public:
  explicit ExampleInstance(PP_Instance instance)
    : pp::Instance(instance),
      callback_factory_(this),
      device_scale_(1.0f),
      send_outstanding_(false){
  }

  virtual ~ExampleInstance() {
  }

 private:
  bool IsConnected();
  bool IsUDP();

  void Connect(std::string host, bool tcp);
  void Close();
  void Send(const std::string& message);
  void Receive(int32_t result);

  void OnConnectCompletion(int32_t result);
  void OnResolveCompletion(int32_t result);
  void OnReceiveCompletion(int32_t result);
  void OnReceiveFromCompletion(int32_t result, pp::NetAddress source);
  void OnSendCompletion(int32_t result);
  bool CreateContext(const pp::Size&);
  void Paint();
  int parserBanner(int cursor);

  /** 
   * 利用libjpeg将缓冲区的JPEG转换成RGB 解压JPEG 
   *  
   * @param[IN]  jpeg_buffer  JPEG图片缓冲区 
   * @param[IN]  jpeg_size    JPEG图片缓冲区大小 
   * @param[IN] rgb_buffer    RGB缓冲区 
   * @param[IN/OUT] size      RGB缓冲区大小 
   * @param[OUT] width        图片宽 
   * @param[OUT] height       图片高 
   * 
   * @return  
   *         0：成功 
   *         -1：打开文件失败 
   * @note 
   *         jpeg、rgb空间由调用者申请，size为输入输出参数，传入为rgb空间大小，转出为rgb实际大小 
   */  
  int jpeg2rgb(char* jpeg_buffer, int jpeg_size, char* rgb_buffer, int* size, int* width, int* height);  


  pp::CompletionCallbackFactory<ExampleInstance> callback_factory_;
  pp::TCPSocket tcp_socket_;
  pp::UDPSocket udp_socket_;
  pp::HostResolver resolver_;
  pp::NetAddress remote_host_;

  pp::Graphics2D context_;
  float device_scale_;
  pp::Size size_;

  char receive_buffer_[kBufferSize];
  char buffer_[kBufferSize];
  int content_cursor_;
  bool send_outstanding_;

  Banner *banner;
  int readBannerBytes;
  int bannerLength;
  int readFrameBytes;
  int frameBodyLength;

public:
  virtual bool Init(uint32_t argc, const char* argn[], const char* argv[]) {
    content_cursor_ = 0;

    banner = new Banner();
    readBannerBytes = 0;
    bannerLength = 2;
    readFrameBytes = 0;
    frameBodyLength = 0;
    //创建2D对象
    pp::Size new_size = pp::Size(1080 * device_scale_,
                                 1920 * device_scale_);

    if (!CreateContext(new_size))
      return false;

    Connect("127.0.0.1:1313",true);

    return true;
  }

};

int ExampleInstance::parserBanner(int cursor) {
  int pid;
  int realWidth;
  int realHeight;
  int virtualWidth;
  int virtualHeight;
  switch (readBannerBytes) {
  case 0:
    // version
    banner->setVersion(receive_buffer_[cursor] & 0xff);
    break;
  case 1:
    // length
    bannerLength = receive_buffer_[cursor] & 0xff;
    banner->setLength(receive_buffer_[cursor] & 0xff);
    break;
  case 2:
  case 3:
  case 4:
  case 5:
    // pid
    pid = banner->getPid();
    pid += ((receive_buffer_[cursor] & 0xff) << ((readBannerBytes - 2) * 8)) ;
    banner->setPid(pid);
    break;
  case 6:
  case 7:
  case 8:
  case 9:
    // real width
    realWidth = banner->getReadWidth();
    realWidth += ((receive_buffer_[cursor] & 0xff) << ((readBannerBytes - 6) * 8)) ;
    banner->setReadWidth(realWidth);
    break;
  case 10:
  case 11:
  case 12:
  case 13:
    // real height
    realHeight = banner->getReadHeight();
    realHeight += ((receive_buffer_[cursor] & 0xff) << ((readBannerBytes - 10) * 8)) ;
    banner->setReadHeight(realHeight);
    break;
  case 14:
  case 15:
  case 16:
  case 17:
    // virtual width
    virtualWidth = banner->getVirtualWidth();
    virtualWidth += ((receive_buffer_[cursor] & 0xff) << ((readBannerBytes - 14) * 8));
    banner->setVirtualWidth(virtualWidth);

    break;
  case 18:
  case 19:
  case 20:
  case 21:
    // virtual height
    virtualHeight = banner->getVirtualHeight();
    virtualHeight += ((receive_buffer_[cursor] & 0xff) << ((readBannerBytes - 18) * 8));
    banner->setVirtualHeight(virtualHeight);
    break;
  case 22:
    // orientation
    banner->setOrientation((receive_buffer_[cursor] & 0xff) * 90);
    break;
  case 23:
    // quirks
    banner->setQuirks(receive_buffer_[cursor] & 0xff);
    break;
  }

  cursor++;
  readBannerBytes++;

  return cursor;
}


bool ExampleInstance::CreateContext(const pp::Size& new_size) {
    const bool kIsAlwaysOpaque = true;
    context_ = pp::Graphics2D(this, new_size, kIsAlwaysOpaque);
    // Call SetScale before BindGraphics so the image is scaled correctly on
    // HiDPI displays.
    context_.SetScale(1.0f / device_scale_);
    if (!BindGraphics(context_)) {
      fprintf(stderr, "Unable to bind 2d context!\n");
      context_ = pp::Graphics2D();
      return false;
    }

    // Allocate a buffer of palette entries of the same size as the new context.
    size_ = new_size;

    return true;
  }

bool ExampleInstance::IsConnected() {
  if (!tcp_socket_.is_null())
    return true;
  if (!udp_socket_.is_null())
    return true;

  return false;
}

bool ExampleInstance::IsUDP() {
  return !udp_socket_.is_null();
}

//初始化链接
void ExampleInstance::Connect(std::string host, bool tcp) {
  if (IsConnected()) {
    PostMessage("Already connected.");
    return;
  }

  if (tcp) {
    if (!pp::TCPSocket::IsAvailable()) {
      PostMessage("TCPSocket not available");
      return;
    }

    tcp_socket_ = pp::TCPSocket(this);
    if (tcp_socket_.is_null()) {
      PostMessage("Error creating TCPSocket.");
      return;
    }
  } else {
    if (!pp::UDPSocket::IsAvailable()) {
      PostMessage("UDPSocket not available");
      return;
    }

    udp_socket_ = pp::UDPSocket(this);
    if (udp_socket_.is_null()) {
      PostMessage("Error creating UDPSocket.");
      return;
    }
  }

  if (!pp::HostResolver::IsAvailable()) {
    PostMessage("HostResolver not available");
    return;
  }

  resolver_ = pp::HostResolver(this);
  if (resolver_.is_null()) {
    PostMessage("Error creating HostResolver.");
    return;
  }

  int port = 1313;
  std::string hostname = host;
  size_t pos = host.rfind(':');
  if (pos != std::string::npos) {
    hostname = host.substr(0, pos);
    port = atoi(host.substr(pos+1).c_str());
  }

  pp::CompletionCallback callback =
      callback_factory_.NewCallback(&ExampleInstance::OnResolveCompletion);
  PP_HostResolver_Hint hint = { PP_NETADDRESS_FAMILY_UNSPECIFIED, 0 };
  resolver_.Resolve(hostname.c_str(), port, hint, callback);
  PostMessage("Resolving ...");
}

//解析host的ip，成功后发起连接
void ExampleInstance::OnResolveCompletion(int32_t result) {
  if (result != PP_OK) {
    PostMessage("Resolve failed.");
    return;
  }

  pp::NetAddress addr = resolver_.GetNetAddress(0);
  PostMessage(std::string("Resolved: ") +
              addr.DescribeAsString(true).AsString());

  pp::CompletionCallback callback =
      callback_factory_.NewCallback(&ExampleInstance::OnConnectCompletion);

  if (IsUDP()) {
    PostMessage("Binding ...");
    remote_host_ = addr;
    PP_NetAddress_IPv4 ipv4_addr = { 0, { 0 } };
    udp_socket_.Bind(pp::NetAddress(this, ipv4_addr), callback);
  } else {
    PostMessage("Connecting ...");
    tcp_socket_.Connect(addr, callback);
  }
}

void ExampleInstance::Close() {
  if (!IsConnected()) {
    PostMessage("Not connected.");
    return;
  }

  if (tcp_socket_.is_null()) {
    udp_socket_.Close();
    udp_socket_ = pp::UDPSocket();
  } else {
    tcp_socket_.Close();
    tcp_socket_ = pp::TCPSocket();
  }

  PostMessage("Closed connection.");
}

//发送数据
void ExampleInstance::Send(const std::string& message) {
  if (!IsConnected()) {
    PostMessage("Not connected.");
    return;
  }

  if (send_outstanding_) {
    PostMessage("Already sending.");
    return;
  }

  uint32_t size = message.size();
  const char* data = message.c_str();
  pp::CompletionCallback callback =
      callback_factory_.NewCallback(&ExampleInstance::OnSendCompletion);
  int32_t result;
  if (IsUDP())
     result = udp_socket_.SendTo(data, size, remote_host_, callback);
  else
     result = tcp_socket_.Write(data, size, callback);
  std::ostringstream status;
  if (result < 0) {
    if (result == PP_OK_COMPLETIONPENDING) {
      status << "Sending bytes: " << size;
      PostMessage(status.str());
      send_outstanding_ = true;
    } else {
      status << "Send returned error: " << result;
      PostMessage(status.str());
    }
  } else {
    status << "Sent bytes synchronously: " << result;
    PostMessage(status.str());
  }
}

//接收到数据
void ExampleInstance::Receive(int32_t result) {
  memset(receive_buffer_, 0, kBufferSize);
  if (IsUDP()) {
    pp::CompletionCallbackWithOutput<pp::NetAddress> callback =
        callback_factory_.NewCallbackWithOutput(
            &ExampleInstance::OnReceiveFromCompletion);
    udp_socket_.RecvFrom(receive_buffer_, kBufferSize, callback);
  } else {
    pp::CompletionCallback callback =
        callback_factory_.NewCallback(&ExampleInstance::OnReceiveCompletion);
    tcp_socket_.Read(receive_buffer_, kBufferSize, callback);
  }
  
}

//连接结束，判断是否连接成功，成功则启动读阻塞
void ExampleInstance::OnConnectCompletion(int32_t result) {
  if (result != PP_OK) {
    std::ostringstream status;
    status << "Connection failed: " << result;
    PostMessage(status.str());
    return;
  }

  if (IsUDP()) {
    pp::NetAddress addr = udp_socket_.GetBoundAddress();
    PostMessage(std::string("Bound to: ") +
                addr.DescribeAsString(true).AsString());
  } else {
    PostMessage("Connected");
  }

  Receive(1);
}

void ExampleInstance::OnReceiveFromCompletion(int32_t result,
                                              pp::NetAddress source) {
  OnReceiveCompletion(result);
}

//一次接受完成，处理之后，启动读阻塞
void ExampleInstance::OnReceiveCompletion(int32_t result) {
  if (result < 0) {
    return;
  }
  bool recieveFlag=true;
  /*合并数据，转换成图片*/
  for (int cursor = 0; cursor < result;) {
    if (readBannerBytes < bannerLength) {
      cursor = parserBanner(cursor);
    } else if (readFrameBytes < 4) {
      // 第二次的缓冲区中前4位数字和为frame的缓冲区大小
      frameBodyLength += ((receive_buffer_[cursor] & 0xff) << (readFrameBytes * 8));
      cursor++;
      readFrameBytes++;

    } else {
      if (result - cursor >= frameBodyLength) {
        for (int i = cursor; i < cursor + frameBodyLength; i++)
        {
          buffer_[content_cursor_++]=receive_buffer_[i];
        }
        cursor += frameBodyLength;
        recieveFlag=false;

        Paint();

        
      } else {
        /*图片数据没有拼凑完*/
        for (int i = cursor; i < result; i++)
        {
          buffer_[content_cursor_++]=receive_buffer_[i];
        }
        frameBodyLength -= result - cursor;
        readFrameBytes += result - cursor;
        cursor = result;
      }
    }
  }
  if(recieveFlag){
    Receive(1);
  }

  return;
}
  
int ExampleInstance::jpeg2rgb(char* jpeg_buffer, int jpeg_size, char* rgb_buffer, int* size, int* width, int* height)  
{  
    struct jpeg_decompress_struct cinfo;  
  
    JSAMPARRAY buffer;  
    int row_stride = 0;  
    char* tmp_buffer = NULL;  
    int rgb_size;  
  
    if (jpeg_buffer == NULL)  
    {  
        printf("no jpeg buffer here.\n");  
        return -1;  
    }  
    if (rgb_buffer == NULL)  
    {  
        printf("you need to alloc rgb buffer.\n");  
        return -1;  
    }  

    jpeg_create_decompress(&cinfo);  
  
    jpeg_mem_src(&cinfo, (const unsigned char *)jpeg_buffer, jpeg_size);  
  
    jpeg_read_header(&cinfo, TRUE);  
  
    //cinfo.out_color_space = JCS_RGB; //JCS_YCbCr;  // 设置输出格式  
  
    jpeg_start_decompress(&cinfo);  
  
    row_stride = cinfo.output_width * cinfo.output_components;  
    *width = cinfo.output_width;  
    *height = cinfo.output_height;  
  
    rgb_size = row_stride * cinfo.output_height; // 总大小  
    if (*size < rgb_size)  
    {  
        printf("rgb buffer to small, we need %d but has only: %d\n", rgb_size, *size);  
    }  
      
    *size = rgb_size;  
  
    buffer = (*cinfo.mem->alloc_sarray)((j_common_ptr) &cinfo, JPOOL_IMAGE, row_stride, 1);  
  
    printf("debug--:\nrgb_size: %d, size: %d w: %d h: %d row_stride: %d \n", rgb_size,  
                cinfo.image_width*cinfo.image_height*3,  
                cinfo.image_width,   
                cinfo.image_height,  
                row_stride);  
    tmp_buffer = rgb_buffer;  
    while (cinfo.output_scanline < cinfo.output_height) // 解压每一行  
    {  
        jpeg_read_scanlines(&cinfo, buffer, 1);  
        // 复制到内存  
        memcpy(tmp_buffer, buffer[0], row_stride);  
        tmp_buffer += row_stride;  
    }  
  
    jpeg_finish_decompress(&cinfo);  
    jpeg_destroy_decompress(&cinfo);  
  
    return 0;  
}  
  


void ExampleInstance::Paint() {
    // See the comment above the call to ReplaceContents below.
    //PP_ImageDataFormat format = pp::ImageData::GetNativeImageDataFormat();
    const bool kDontInitToZero = false;
    int buffer_size=kBufferSize;
    int img_width,img_height;
    //创建2D对象
    pp::Size _size_ = pp::Size(banner->getVirtualWidth() * device_scale_,
                                 banner->getVirtualHeight() * device_scale_);
    img_width=_size_.width();
    img_height=_size_.height();

    pp::ImageData image_data(this, PP_IMAGEDATAFORMAT_RGBA_PREMUL, _size_, kDontInitToZero);

    char* data = static_cast<char*>(image_data.data());
    if (!data)
      return;

    jpeg2rgb(buffer_,content_cursor_,data,&buffer_size,&img_width,&img_height);

        std::ostringstream status;
        status << "图片内容: " << banner->getVirtualWidth();
        PostMessage(status.str());

    context_.ReplaceContents(&image_data);

    pp::CompletionCallback callback =
        callback_factory_.NewCallback(&ExampleInstance::Receive);

    memset(buffer_, 0, kBufferSize);
    readFrameBytes=0;
    frameBodyLength=0;
    content_cursor_=0;
    context_.Flush(callback);
  }

//发送完成，抛出结果
void ExampleInstance::OnSendCompletion(int32_t result) {
  std::ostringstream status;
  if (result < 0) {
    status << "Send failed with: " << result;
  } else {
    status << "Sent bytes: " << result;
  }
  send_outstanding_ = false;
  PostMessage(status.str());
}

// The ExampleModule provides an implementation of pp::Module that creates
// ExampleInstance objects when invoked.
class ExampleModule : public pp::Module {
 public:
  ExampleModule() : pp::Module() {}
  virtual ~ExampleModule() {}

  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new ExampleInstance(instance);
  }
};

// Implement the required pp::CreateModule function that creates our specific
// kind of Module.
namespace pp {
Module* CreateModule() { return new ExampleModule(); }
}  // namespace pp


