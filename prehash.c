#include	<windows.h>
#include	<winreg.h>
#include	<tchar.h>
#include	<tlhelp32.h>
#include	<shellapi.h>
#include	<strsafe.h>
#include	"version.h"

typedef struct _option{
	BOOL AsCUI;
	BOOL IsCUI;
	BOOL ShowHelp;
	BOOL DispFilename;
	BOOL DispAsUnicode;
	BOOL HashVista;
	BOOL HashXp;
} OPTIONS, *LPOPTIONS;

VOID (*my_puts)(HANDLE, LPCTSTR);
HANDLE hStderr = INVALID_HANDLE_VALUE;
HANDLE hStdout = INVALID_HANDLE_VALUE;

LPTSTR ToUpperCase( LPTSTR lpszText )
{
	LPTSTR s = lpszText;
	while( *s ){
		if( _T('a') <= *s && *s <= _T('z' ) ) *s = *s - _T('a') + _T('A');
		s++;
	}
	return lpszText;
}

LPCTSTR Basename( LPCTSTR lpszPath )
{
	LPCTSTR p = lpszPath;
	LPCTSTR r = lpszPath;

	while( *p ){
		while( *p == _T('\\') ){
			p++;
			if( *p != _T('\\') ) r = p;
		}
		p++;
	}
	return r;

}


DWORD GetParentProcessName( LPTSTR lpszParentName, DWORD cchSize )
{
	HANDLE hSnapshot; 
	PROCESSENTRY32 pe32;
	DWORD pid, ppid = 0;
	DWORD r = 0;
	size_t w;

	pid = GetCurrentProcessId();

	hSnapshot = CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0 );
	__try{
		if( hSnapshot == INVALID_HANDLE_VALUE ) __leave;

		ZeroMemory( &pe32, sizeof( pe32 ) );
		pe32.dwSize = sizeof( pe32 );
		if( !Process32First( hSnapshot, &pe32 ) ) __leave;

		// find my process and get parent's pid
		do{
			if( pe32.th32ProcessID == pid ){
				ppid = pe32.th32ParentProcessID;
				break;
			}
		}while( Process32Next( hSnapshot, &pe32 ) );

		if( ppid == 0 ) __leave;		// not found

		// rewind
		ZeroMemory( &pe32, sizeof( pe32 ) );
		pe32.dwSize = sizeof( pe32 );
		if( !Process32First( hSnapshot, &pe32 ) ) __leave;

		// find parrent process and get process name
		do{
			if( pe32.th32ProcessID == ppid ){
				StringCchCopy( lpszParentName, cchSize, pe32.szExeFile );
				if( StringCchLength( pe32.szExeFile, 32767, &w ) == NO_ERROR ){
					r = (DWORD)w;
				}
				break;
			}
		}while( Process32Next( hSnapshot, &pe32 ) );
	}
	__finally{
		if( hSnapshot != INVALID_HANDLE_VALUE ) CloseHandle( hSnapshot );
	}
	return r;


}

/* return TRUE if parent process is CMD.EXE */
BOOL IsCommandLine()
{
	TCHAR buf[ MAX_PATH ];
	TCHAR *p;
	DWORD r;

	r = GetParentProcessName( buf, _countof( buf ) );
	if( r < _countof( buf ) ){
		p = buf;
		do{
			if( CompareString( LOCALE_USER_DEFAULT, SORT_STRINGSORT | NORM_IGNORECASE, p, -1, _T("cmd.exe"), -1 ) == CSTR_EQUAL ){
				return TRUE;
			}
			while( *p && *p != _T('\\') && *p != _T('/' ) )p++;
			if( *p == _T('\\') || *p == _T('/') ) p++;
		}while( *p );
	}
	return FALSE;
}

VOID my_putsG( HANDLE h, LPCTSTR s )
{
	UINT n = MB_OK;
	if( h == hStderr && h != INVALID_HANDLE_VALUE ){
		n |= MB_ICONSTOP;
	}else{
		n |= MB_ICONINFORMATION;
	}
	MessageBox( HWND_DESKTOP, s, _T("prehash"), n );

}

// Console, ANSI
VOID my_putsCA( HANDLE h, LPCTSTR s )
{
	DWORD n1, n2;
	DWORD len = 0;
	LPSTR p;

#ifdef UNICODE
	UINT cp = GetConsoleOutputCP();

	if( ( len = WideCharToMultiByte( cp, 0, s, -1, NULL, 0, NULL, NULL ) ) == 0 ) return;
	if( ( p = (LPSTR)LocalAlloc( LMEM_FIXED, len ) ) == NULL ) return;
	len = WideCharToMultiByte( cp, 0, s, -1, p, len, NULL, NULL );
#else
	size_t n;
	p = (LPTSTR)s;
	if( StringCbLength( p, 4096, &n ) != S_OK ) len = 0;
	else len = n;
#endif

	n1 = len ? len -1 : 0;
	while( n1 ){
		if( !WriteFile( h, p, n1, &n2, NULL ) )  break;
		n1 -= n2;
	}
#ifdef UNICODE
	LocalFree( p );
#endif
}

// Console, Wide
VOID my_putsCW( HANDLE h, LPCTSTR s )
{
	DWORD n1, n2;
	DWORD len = 0;
	LPWSTR p;
	size_t n;

#ifdef UNICODE
	p = (LPWSTR)s;
	if( (len = StringCbLength( p, 4096, &n ) ) != S_OK ) len = 0;
	else len = n;
#else
	if( ( len = MultiByteToWideChar( CP_ACP, 0, s, -1, NULL, 0 ) ) == 0 ) return;
	if( ( p = (LPWSTR)LocalAlloc( LMEM_FIXED, len ) ) == NULL ) return;
	len = MultiByteToWideChar( CP_ACP, 0, s, -1, p, len );
#endif
	n1 = len ? len - 1 : 0;
	while( n1 ){
		if( !WriteFile( h, p, n1, &n2, NULL ) )  break;
		n1 -= n2;
	}
#ifndef UNICODE
	LocalFree( p );
#endif
}

VOID my_printf( HANDLE h, LPCTSTR lpszFormat, ... )
{
	va_list ap;
	LPTSTR buf;
	DWORD r;

	va_start( ap, lpszFormat );
	r = FormatMessage(
			FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_STRING,
			(LPCVOID)lpszFormat,
			0, 0, (LPTSTR)&buf, 0,
			&ap );
	if( r ) my_puts( h, buf );

	va_end( ap );
	LocalFree( buf );
}

void ShowError(DWORD dwErrorCode, LPCTSTR s)
{
	LPVOID p1 = NULL, p2 = NULL;
	DWORD r;
	TCHAR buf[ 1024 ];
	DWORD_PTR pArgs[ 2 ];

	__try{
		r = FormatMessage( 
			FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, 
			NULL, dwErrorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPTSTR) &p1, 0,NULL);
		if( r ){
			if( s ){
				pArgs[ 0 ] = (DWORD_PTR)s;
				pArgs[ 1 ] = (DWORD_PTR)p1;
				r = FormatMessage(
					FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_STRING | FORMAT_MESSAGE_ARGUMENT_ARRAY,
					(LPCVOID)_T("%1!s!:%2!s!"),
					0,
					0,
					(LPTSTR)&p2,
					0,
					(va_list*)pArgs );
				if( r ){
					my_puts( hStderr, (LPCTSTR)p2 );
				}else{
					my_puts( hStderr, (LPCTSTR)p1 );
				}
			}else{
				my_puts( hStderr, (LPCTSTR)p1 );
			}
		}else{
			StringCchPrintf( buf, _countof( buf ), s ? _T("Error:%lu\n%s\n") : _T("Error:%lu\n"), dwErrorCode, s );
			my_puts( hStderr, buf ); 
		}
	}
	__finally{
		if( p1 ) LocalFree( p1 );
		if( p2 ) LocalFree( p2 );
	}
}

VOID ShowHelp( VOID )
{
	LPCTSTR s = 
		_T( "\nprehash ver %1!s! - obtain the hash of Windows prefetch.\n\n" )
		_T( "Usage:  prehash [-c|-w] [-kuxv] filename\n\n" )
		_T( "        -c : run as console program.\n" )
		_T( "        -w : run as GUI program.\n" )
		_T( "        -k : display filename prior to hash value.\n" )
		_T( "        -u : display with Unicode.\n" )
		_T( "        -x : calc hash value for XP.\n" )
		_T( "        -v : calc hash value for Vista.\n" );
	my_printf( hStdout, s, APP_VERSION );

	return ;
}

DWORD HashXp( LPCWSTR lpszPath )
{
	LONG r = 0;
	WCHAR c;

	while( *lpszPath ){
		c = *lpszPath;
		//if( L'a' <= c && c <= L'z' ) c = (WCHAR)CharUpper( (LPWSTR)c );
		if( c <= 0xff )
			r = 37 * ( ( 37 * r ) + c ); 
		else
			r = 37 * ( ( 37 * r ) + ( c / 256 ) ) + ( c % 256 );
		lpszPath++;
	}
    r *= 314159269;
    if ( r < 0 ) r = -r;
    r %= 1000000007;
    return r;
}

DWORD HashVista( LPCWSTR lpszPath )
{
	DWORD r = 314159;
	WCHAR c;
	while( *lpszPath ){
		c = *lpszPath;
		//if( L'a' <= c && c <= L'z' ) c = (WCHAR)CharUpper( (LPWSTR)c );
		if( c <= 0xff )
			r = 37 * ( ( 37 * r ) + c ); 
		else
			r = 37 * ( ( 37 * r ) + ( c / 256 ) ) + ( c % 256 );
		lpszPath++;
	}
	return r;
}

BOOL PutHash( LPCTSTR lpszFile, LPOPTIONS lpOpt )
{
	DWORD w;
	LPCTSTR lpszFormat;

	if( lpOpt->DispFilename ){
		lpszFormat = _T( "%3!s!(%4!s!) : %1!s!-%2!8.8X!.pf\n" );
	}else{
		if( lpOpt->HashXp && lpOpt->HashVista ){
			lpszFormat = _T( "(%4!s!) : %1!s!-%2!8.8X!.pf\n" );
		}else{
			lpszFormat = _T( "%1!s!-%2!8.8X!.pf\n" );
		}
	}

	if( lpOpt->HashVista ){
		w = HashVista( lpszFile );
		my_printf( hStdout, lpszFormat, Basename( lpszFile ), w, lpszFile, _T("Vista") );
	}
	if( lpOpt->HashXp ){
		w = HashXp( lpszFile );
		my_printf( hStdout, lpszFormat, Basename( lpszFile ), w, lpszFile, _T("XP") );
	}
	return TRUE;


}

int _tmain( int argc, TCHAR**argv )
//int WINAPI WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow )
{
	int nArgs;
	LPWSTR *lplpszArgs;
	int i, n = 0;
	LPTSTR p;
	OPTIONS opt;

	ZeroMemory( &opt, sizeof( opt ) );

	opt.IsCUI = opt.AsCUI = IsCommandLine();
	lplpszArgs = CommandLineToArgvW( GetCommandLineW(), &nArgs );
	if( lplpszArgs ){
		for( i = 1; i < nArgs; i++ ){
			p = lplpszArgs[ i ];
			if( *p == _T('-') || *p == _T('/') ){
				*p = '\0';
				p++;
				while( *p ){
					switch( *p ){
						case _T('h'):
						case _T('H'):
						case _T('?'):
							opt.ShowHelp = TRUE;
							break;
						case _T('c'):
						case _T('C'):
							opt.AsCUI = TRUE;
							break;
						case _T('w'):
						case _T('W'):
							opt.AsCUI = FALSE;
							break;
						case _T('k'):
						case _T('K'):
							opt.DispFilename = TRUE;
							break;
						case _T('u'):
						case _T('U'):
							opt.DispAsUnicode = TRUE;
							break;
						case _T('v'):
						case _T('V'):
							opt.HashVista = TRUE;
							break;
						case _T('x'):
						case _T('X'):
							opt.HashXp = TRUE;
							break;
					}
					p++;
				}
			}else{
				ToUpperCase( p );
				n++;
			}
		}
	}else{
		return -1;
	}
	if( n == 0 ) opt.ShowHelp = TRUE;

	if( !opt.HashXp && !opt.HashVista ){
		opt.HashVista = TRUE;
		opt.HashXp = TRUE;
	}

	if( opt.AsCUI ){
		/*
		if( !AttachConsole( ATTACH_PARENT_PROCESS ) ){
			MessageBox( 0, _T("cannot attach to console."), _T("prehash"), MB_OK | MB_ICONSTOP );
			return -1;
		}
		hStdout = CreateFile( _T("CONOUT$"), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, 0);
		hStderr = CreateFile( _T("CONOUT$"), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, 0);
		my_puts = my_putsC;
		*/
		hStdout = GetStdHandle( STD_OUTPUT_HANDLE );
		hStderr = GetStdHandle( STD_ERROR_HANDLE );
		if( opt.DispAsUnicode ) my_puts = my_putsCW;
		else my_puts = my_putsCA;
	}else{
		hStderr = GetStdHandle( STD_ERROR_HANDLE );
		hStdout = GetStdHandle( STD_OUTPUT_HANDLE );
		my_puts = my_putsG;
	}

	if( opt.ShowHelp ){
		ShowHelp();
		return 0;
	}
	if( !opt.IsCUI )
		//ShowWindow( GetConsoleWindow(), SW_HIDE );
		FreeConsole();

	for( i = 1; i < nArgs; i++ ){
		p = lplpszArgs[ i ];
		if( !p || !*p ) continue;
		PutHash( p, &opt );
	}
	if( lplpszArgs ) LocalFree( lplpszArgs );

	return 0;
}


