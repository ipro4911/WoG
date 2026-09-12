// kitf_common.h - shared helpers for the KITF launcher/updater tools.
// Public domain-ish, self-contained, no external deps beyond Win32.
#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wininet.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iterator>

#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "comctl32.lib")

// ---------------------------------------------------------------------------
// MD5 (compact public-domain implementation) - used for file verification
// ---------------------------------------------------------------------------
struct MD5 {
	static unsigned long rol(unsigned long x,int c){return (x<<c)|(x>>(32-c));}
	static unsigned long F(unsigned long x,unsigned long y,unsigned long z){return (x&y)|(~x&z);}
	static unsigned long G(unsigned long x,unsigned long y,unsigned long z){return (x&z)|(y&~z);}
	static unsigned long H(unsigned long x,unsigned long y,unsigned long z){return x^y^z;}
	static unsigned long I(unsigned long x,unsigned long y,unsigned long z){return y^(x|~z);}
	static const unsigned long K[64];
	static const int S[64];
	static std::string hex(unsigned long v){
		static const char* hx="0123456789abcdef";
		char b[9];
		for(int i=0;i<4;i++){
			unsigned char bl=(unsigned char)((v>>(8*i))&0xff);
			b[i*2]  =hx[(bl>>4)&0xf];
			b[i*2+1]=hx[bl&0xf];
		}
		b[8]=0;return std::string(b);
	}
	std::string hash(const unsigned char* data, unsigned long long len){
		std::vector<unsigned char> msg(data,data+len);
		unsigned long long bitlen=len*8;
		msg.push_back(0x80);
		while((msg.size()%64)!=56)msg.push_back(0);
		for(int i=0;i<8;i++)msg.push_back((unsigned char)((bitlen>>(8*i))&0xff));
		unsigned long a0=0x67452301,b0=0xefcdab89,c0=0x98badcfe,d0=0x10325476;
		unsigned long a=a0,b=b0,c=c0,d=d0;
		for(size_t i=0;i<msg.size();i+=64){
			unsigned long M[16];
			for(int j=0;j<16;j++)
				M[j]=(unsigned long)msg[i+j*4] | ((unsigned long)msg[i+j*4+1]<<8) |
					((unsigned long)msg[i+j*4+2]<<16) | ((unsigned long)msg[i+j*4+3]<<24);
			unsigned long A=a,B=b,C=c,D=d;
			for(int j=0;j<64;j++){
				unsigned long f;int g;
				if(j<16){f=F(B,C,D);g=j;}
				else if(j<32){f=G(B,C,D);g=(5*j+1)%16;}
				else if(j<48){f=H(B,C,D);g=(3*j+5)%16;}
				else {f=I(B,C,D);g=(7*j)%16;}
				unsigned long tmp=D;D=C;C=B;
				B=B+rol(A+f+K[j]+M[g],S[j]);
				A=tmp;
			}
			a+=A;b+=B;c+=C;d+=D;
		}
		return hex(a)+hex(b)+hex(c)+hex(d);
	}
};
const unsigned long MD5::K[64]={
0xd76aa478,0xe8c7b756,0x242070db,0xc1bdceee,0xf57c0faf,0x4787c62a,0xa8304613,0xfd469501,
0x698098d8,0x8b44f7af,0xffff5bb1,0x895cd7be,0x6b901122,0xfd987193,0xa679438e,0x49b40821,
0xf61e2562,0xc040b340,0x265e5a51,0xe9b6c7aa,0xd62f105d,0x02441453,0xd8a1e681,0xe7d3fbc8,
0x21e1cde6,0xc33707d6,0xf4d50d87,0x455a14ed,0xa9e3e905,0xfcefa3f8,0x676f02d9,0x8d2a4c8a,
0xfffa3942,0x8771f681,0x6d9d6122,0xfde5380c,0xa4beea44,0x4bdecfa9,0xf6bb4b60,0xbebfbc70,
0x289b7ec6,0xeaa127fa,0xd4ef3085,0x04881d05,0xd9d4d039,0xe6db99e5,0x1fa27cf8,0xc4ac5665,
0xf4292244,0x432aff97,0xab9423a7,0xfc93a039,0x655b59c3,0x8f0ccc92,0xffeff47d,0x85845dd1,
0x6fa87e4f,0xfe2ce6e0,0xa3014314,0x4e0811a1,0xf7537e82,0xbd3af235,0x2ad7d2bb,0xeb86d391};
const int MD5::S[64]={
7,12,17,22,7,12,17,22,7,12,17,22,7,12,17,22,
5,9,14,20,5,9,14,20,5,9,14,20,5,9,14,20,
4,11,16,23,4,11,16,23,4,11,16,23,4,11,16,23,
6,10,15,21,6,10,15,21,6,10,15,21,6,10,15,21};

// ---------------------------------------------------------------------------
// Manifest format (line based, one entry per line):
//   KITFUP 1
//   VER <appversion>
//   FILE <path>|<remotesize>|<md5>|<remoteurl>
//   ... and an EOF marker
// ---------------------------------------------------------------------------
struct ManifestEntry {
	std::string path;      // local relative path e.g. "client/ogremagix.exe"
	long long size;        // remote size in bytes
	std::string md5;       // hex md5 of the file
	std::string url;       // full remote URL
};

struct Manifest {
	std::string appVersion;
	std::vector<ManifestEntry> files;
	bool parse(const std::string& text, const std::string& baseUrl){
		files.clear();
		std::istringstream iss(text);
		std::string line;
		while(std::getline(iss,line)){
			if(line.empty())continue;
			if(line.rfind("KITFUP",0)==0)continue;
			if(line.rfind("VER ",0)==0){appVersion=line.substr(4);continue;}
			if(line.rfind("FILE ",0)==0){
				std::string rest=line.substr(5);
				size_t p1=rest.find('|'); if(p1==std::string::npos)continue;
				size_t p2=rest.find('|',p1+1); if(p2==std::string::npos)continue;
				size_t p3=rest.find('|',p2+1); if(p3==std::string::npos)continue;
				ManifestEntry e;
				e.path=rest.substr(0,p1);
				e.size=atoll(rest.substr(p1+1,p2-p1-1).c_str());
				e.md5=rest.substr(p2+1,p3-p2-1);
				std::string rel=rest.substr(p3+1);
				e.url = baseUrl;
				if(!baseUrl.empty() && baseUrl.back()!='/')e.url+='/';
				e.url+=rel;
				files.push_back(e);
			}
		}
		return !files.empty() || !appVersion.empty();
	}
};

// ---------------------------------------------------------------------------
// HTTP GET (WinINet)
// ---------------------------------------------------------------------------
static bool httpGet(const std::string& url, std::string& out, unsigned long* status=0){
	HINTERNET hNet = InternetOpenA("KITFUpdater", INTERNET_OPEN_TYPE_PRECONFIG, 0,0,0);
	if(!hNet)return false;
	DWORD timeout=8000;
	InternetSetOptionA(hNet,INTERNET_OPTION_CONNECT_TIMEOUT,&timeout,sizeof(timeout));
	InternetSetOptionA(hNet,INTERNET_OPTION_RECEIVE_TIMEOUT,&timeout,sizeof(timeout));
	InternetSetOptionA(hNet,INTERNET_OPTION_SEND_TIMEOUT,&timeout,sizeof(timeout));
	DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_SECURE;
	// if not https, drop secure flag
	if(url.rfind("https://",0)!=0 && url.rfind("http://",0)!=0) flags &= ~INTERNET_FLAG_SECURE;
	if(url.rfind("http://",0)==0) flags &= ~INTERNET_FLAG_SECURE;
	HINTERNET hOpen = InternetOpenUrlA(hNet, url.c_str(), 0,0, flags,0);
	if(!hOpen){InternetCloseHandle(hNet);return false;}
	char buf[4096];DWORD rd=0;
	while(InternetReadFile(hOpen,buf,sizeof(buf),&rd) && rd>0){
		out.append(buf,rd);
	}
	if(status){DWORD len=sizeof(DWORD);HttpQueryInfoA(hOpen,HTTP_QUERY_STATUS_CODE|HTTP_QUERY_FLAG_NUMBER,status,&len,0);}
	InternetCloseHandle(hOpen);
	InternetCloseHandle(hNet);
	return !out.empty();
}

// ---------------------------------------------------------------------------
// HTTP POST (WinINet) - for account registration etc.
// Posts form-url-encoded body, returns response body in out, HTTP status in *status.
// ---------------------------------------------------------------------------
static bool httpPost(const std::string& url, const std::string& body,
                     std::string& out, unsigned long* status=0){
	HINTERNET hNet = InternetOpenA("KITFLauncher", INTERNET_OPEN_TYPE_PRECONFIG, 0,0,0);
	if(!hNet)return false;
	DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE;
	if(url.rfind("https://",0)==0) flags |= INTERNET_FLAG_SECURE;
	else flags &= ~INTERNET_FLAG_SECURE;
	// Parse host + port + path from URL
	std::string host,path;
	int port=INTERNET_DEFAULT_HTTP_PORT;
	{
		size_t hp=url.find("://");
		hp=(hp==std::string::npos)?0:hp+3;
		size_t sl=url.find('/',hp);
		std::string hostport=url.substr(hp,sl-hp);
		path=(sl==std::string::npos)?"/":url.substr(sl);
		if(url.rfind("https://",0)==0)port=INTERNET_DEFAULT_HTTPS_PORT;
		size_t colon=hostport.rfind(':');
		if(colon!=std::string::npos){
			host=hostport.substr(0,colon);
			port=atoi(hostport.substr(colon+1).c_str());
			if(port<=0)port=(url.rfind("https://",0)==0)?INTERNET_DEFAULT_HTTPS_PORT:INTERNET_DEFAULT_HTTP_PORT;
		}else host=hostport;
	}
	HINTERNET hConn = InternetConnectA(hNet,host.c_str(),(INTERNET_PORT)port,0,0,INTERNET_SERVICE_HTTP,0,0);
	if(!hConn){InternetCloseHandle(hNet);return false;}
	HINTERNET hReq=HttpOpenRequestA(hConn,"POST",path.c_str(),0,0,0,flags,0);
	if(!hReq){InternetCloseHandle(hConn);InternetCloseHandle(hNet);return false;}
	const char* hdr="Content-Type: application/x-www-form-urlencoded\r\n";
	if(!HttpSendRequestA(hReq,hdr,(DWORD)strlen(hdr),(LPVOID)body.c_str(),(DWORD)body.size())){
		InternetCloseHandle(hReq);InternetCloseHandle(hConn);InternetCloseHandle(hNet);return false;
	}
	unsigned long gotStatus=0;
	{DWORD len=sizeof(DWORD);HttpQueryInfoA(hReq,HTTP_QUERY_STATUS_CODE|HTTP_QUERY_FLAG_NUMBER,&gotStatus,&len,0);}
	if(status)*status=gotStatus;
	char buf[4096];DWORD rd=0;
	while(InternetReadFile(hReq,buf,sizeof(buf),&rd) && rd>0) out.append(buf,rd);
	InternetCloseHandle(hReq);InternetCloseHandle(hConn);InternetCloseHandle(hNet);
	// Only treat it as success if we got a 2xx response AND non-HTML body.
	if(gotStatus<200||gotStatus>=300)return false;
	if(!out.empty()&&out[0]=='<')return false;
	return !out.empty();
}

// Download a URL to a local file. Returns bytes written, or -1 on failure.
// progressCb(label, total, done) may return false to abort.
// httpStat (optional) receives the HTTP status code, or 0 if the request
// never got a response / failed locally (fopen, rename, ...).
static long long downloadToFile(const std::string& url, const std::string& outPath,
	bool (*progressCb)(const char*, long long, long long)=0, bool* abort=0,
	unsigned long* httpStat=0){
	std::string tmpOut = outPath + ".part";
	HINTERNET hNet = InternetOpenA("KITFUpdater", INTERNET_OPEN_TYPE_PRECONFIG, 0,0,0);
	if(!hNet){if(httpStat)*httpStat=0;return -1;}
	DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE;
	if(url.rfind("https://",0)==0) flags |= INTERNET_FLAG_SECURE;
	HINTERNET hOpen = InternetOpenUrlA(hNet, url.c_str(), 0,0,flags,0);
	if(!hOpen){InternetCloseHandle(hNet);if(httpStat)*httpStat=0;return -1;}
	DWORD status=0,len=sizeof(DWORD);
	HttpQueryInfoA(hOpen,HTTP_QUERY_STATUS_CODE|HTTP_QUERY_FLAG_NUMBER,&status,&len,0);
	if(httpStat)*httpStat=status;
	if(status!=200){InternetCloseHandle(hOpen);InternetCloseHandle(hNet);return -1;}
	char hbuf[64];DWORD hblen=64;
	long long total=-1;
	if(HttpQueryInfoA(hOpen,HTTP_QUERY_CONTENT_LENGTH,hbuf,&hblen,0))total=atoll(hbuf);
	FILE* f;
	if(fopen_s(&f,tmpOut.c_str(),"wb")!=0){InternetCloseHandle(hOpen);InternetCloseHandle(hNet);return -1;}
	char buf[65536];DWORD rd=0;long long done=0;
	bool ok=true;
	while(InternetReadFile(hOpen,buf,sizeof(buf),&rd) && rd>0){
		fwrite(buf,1,rd,f);
		done+=rd;
		if(progressCb){std::string lbl=outPath;size_t s=lbl.find_last_of("/\\");if(s!=std::string::npos)lbl.erase(0,s+1);ok=progressCb(lbl.c_str(),total,done);}
		if((abort&&*abort)||!ok)break;
	}
	fclose(f);
	InternetCloseHandle(hOpen);
	InternetCloseHandle(hNet);
	if(!ok||(abort&&*abort)){remove(tmpOut.c_str());return -1;}
	// NOTE: CRT rename() on Windows FAILS if outPath already exists (POSIX
	// would overwrite). The standalone updater used to download straight to
	// the live path, so every update of an existing file failed here - and
	// the caller then deleted the good local file. Use Win32 replace
	// semantics so an existing destination is overwritten atomically.
	SetFileAttributesA(outPath.c_str(),FILE_ATTRIBUTE_NORMAL);
	if(!MoveFileExA(tmpOut.c_str(),outPath.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){
		// Fallback for cross-volume / odd cases: remove dest then rename.
		remove(outPath.c_str());
		if(rename(tmpOut.c_str(),outPath.c_str())!=0){remove(tmpOut.c_str());return -1;}
	}
	return done;
}

// ---------------------------------------------------------------------------
// File helpers
// ---------------------------------------------------------------------------
static bool fileExists(const std::string& p){
	DWORD a=GetFileAttributesA(p.c_str());
	return a!=INVALID_FILE_ATTRIBUTES && !(a&FILE_ATTRIBUTE_DIRECTORY);
}
static long long fileSize(const std::string& p){
	WIN32_FILE_ATTRIBUTE_DATA d;
	if(GetFileAttributesExA(p.c_str(),GetFileExInfoStandard,&d))return ((long long)d.nFileSizeHigh<<32)|d.nFileSizeLow;
	return -1;
}
static std::string fileMd5(const std::string& p){
	std::ifstream f(p.c_str(),std::ios::binary);
	if(!f.is_open())return "";
	std::vector<unsigned char> data((std::istreambuf_iterator<char>(f)),std::istreambuf_iterator<char>());
	if(data.empty()){static unsigned char dummy=0;return MD5().hash(&dummy,0);}
	return MD5().hash(&data[0],data.size());
}
static bool ensureDirs(const std::string& path){
	std::string cur;
	size_t start=0;
	while(start<path.size()){
		size_t slash=path.find_first_of("/\\",start);
		if(slash==std::string::npos)break;
		cur+=path.substr(start,slash-start+1);
		CreateDirectoryA(cur.c_str(),0);
		start=slash+1;
	}
	return true;
}
