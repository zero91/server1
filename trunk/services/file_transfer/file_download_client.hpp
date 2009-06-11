// http://code.google.com/p/server1/
//
// You can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// Author: xiliu.tang@gmail.com (Xiliu Tang)

#ifndef FILE_DOWNLOAD_CLIENT_HPP_
#define FILE_DOWNLOAD_CLIENT_HPP_
class FileDownloadClient {
 public:
  FileDownloadClient(
      const string &src_filename,
      const string &local_filename,
      int thread);
  void Start();
  void Stop();
  // The percent * 1000, 1000 means transfer finished.
  int Percent();
  void Wait();
 private:
};
#endif  // FILE_DOWNLOAD_CLIENT_HPP_
