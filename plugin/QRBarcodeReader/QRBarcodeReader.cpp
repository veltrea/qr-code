//
//  QRBarcodeReader.cpp
//  QRBarcodeReader.fmplugin — FileMaker 外部関数プラグイン
//

#include "FMWrapper/FMXTypes.h"
#include "FMWrapper/FMXText.h"
#include "FMWrapper/FMXData.h"
#include "FMWrapper/FMXCalcEngine.h"
#include "FMWrapper/FMXBinaryData.h"

// stb_image
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#include "stb_image.h"

// ZXing-C++
#include "ReadBarcode.h"
#include "BarcodeFormat.h"

// nlohmann/json
#include "nlohmann/json.hpp"

#include <string>
#include <vector>

// プラグイン識別情報(SPEC.md 3.4 — 確定後は変更禁止)
static const char* kQRB_PluginID( "QRBc" );
static const char* kQRB_PluginName( "QRBarcodeReader" );
static const char* kQRB_PluginDescription( "Reads QR codes and barcodes from container field images" );
static const char* kQRB_VersionString( "1.0.0" );

// QRB_Version(関数 ID 100)
enum { kQRB_VersionID = 100, kQRB_VersionMin = 0, kQRB_VersionMax = 0 };
static const char* kQRB_VersionName( "QRB_Version" );
static const char* kQRB_VersionDefinition( "QRB_Version" );
static const char* kQRB_VersionDescription( "Returns the plug-in version string" );

// QRB_Decode(関数 ID 101)
enum { kQRB_DecodeID = 101, kQRB_DecodeMin = 1, kQRB_DecodeMax = 1 };
static const char* kQRB_DecodeName( "QRB_Decode" );
static const char* kQRB_DecodeDefinition( "QRB_Decode( container )" );
static const char* kQRB_DecodeDescription( "Returns the text of the first detected code in the container" );

// QRB_DecodeAll(関数 ID 102)
enum { kQRB_DecodeAllID = 102, kQRB_DecodeAllMin = 1, kQRB_DecodeAllMax = 1 };
static const char* kQRB_DecodeAllName( "QRB_DecodeAll" );
static const char* kQRB_DecodeAllDefinition( "QRB_DecodeAll( container )" );
static const char* kQRB_DecodeAllDescription( "Returns a JSON array of all detected codes in the container" );

// QRB_LastError(関数 ID 103)
enum { kQRB_LastErrorID = 103, kQRB_LastErrorMin = 0, kQRB_LastErrorMax = 0 };
static const char* kQRB_LastErrorName( "QRB_LastError" );
static const char* kQRB_LastErrorDefinition( "QRB_LastError" );
static const char* kQRB_LastErrorDescription( "Returns the last error code and message" );

// スレッドローカルなエラー変数
thread_local std::string g_lastError;

static void SetLastError(int code, const std::string& message)
{
	if (code == 0)
	{
		g_lastError.clear();
	}
	else
	{
		g_lastError = std::to_string(code) + ": " + message;
	}
}

// 共通ヘルパー: コンテナフィールドから画像のバイナリデータを抽出する
static fmx::errcode ExtractImageFromContainer( const fmx::DataVect& parms, std::vector<unsigned char>& imageBuffer )
{
	if (parms.Size() < 1)
	{
		SetLastError(1, "Container parameter is missing");
		return fmx::ExprEnv::kPluginErrResult1;
	}

	const fmx::Data& dat = parms.At(0);
	if (dat.GetNativeType() != fmx::Data::kDTBinary)
	{
		SetLastError(1, "Parameter is not a container field");
		return fmx::ExprEnv::kPluginErrResult1;
	}

	const fmx::BinaryData& bin = parms.AtAsBinaryData(0);
	if (bin.GetCount() == 0)
	{
		SetLastError(1, "Container is empty");
		return fmx::ExprEnv::kPluginErrResult1;
	}

	// 探索の優先順: JPEG -> PNGf -> GIFf -> BMPf -> FILE
	const char* types[5] = { "JPEG", "PNGf", "GIFf", "BMPf", "FILE" };
	fmx::int32 idx = -1;
	for (int i = 0; i < 5; ++i)
	{
		fmx::QuadCharUniquePtr qtype( types[i][0], types[i][1], types[i][2], types[i][3] );
		idx = bin.GetIndex(*qtype);
		if (idx >= 0)
		{
			break;
		}
	}

	if (idx < 0)
	{
		SetLastError(1, "Unsupported container stream format (must be image)");
		return fmx::ExprEnv::kPluginErrResult1;
	}

	fmx::uint32 size = bin.GetSize(idx);
	if (size == 0)
	{
		SetLastError(1, "Image stream size is zero");
		return fmx::ExprEnv::kPluginErrResult1;
	}

	imageBuffer.resize(size);
	bin.GetData(idx, 0, size, imageBuffer.data());
	return 0;
}

// Do_QRB_Version ==========================================================================
static FMX_PROC(fmx::errcode) Do_QRB_Version( short /* funcId */, const fmx::ExprEnv& /* environment */, const fmx::DataVect& /* dataVect */, fmx::Data& results )
{
	fmx::errcode errorResult( 960 );

	try
	{
		fmx::TextUniquePtr outText;
		outText->Assign( kQRB_VersionString, fmx::Text::kEncoding_UTF8 );
		results.SetAsText( *outText, results.GetLocale() );
		errorResult = 0;
	}
	catch (...)
	{
		errorResult = 960;
	}

	return errorResult;
}

// Do_QRB_Decode ===========================================================================
static FMX_PROC(fmx::errcode) Do_QRB_Decode( short /* funcId */, const fmx::ExprEnv& /* environment */, const fmx::DataVect& parms, fmx::Data& results )
{
	SetLastError(0, ""); // エラー状態のクリア
	fmx::errcode errorResult( 960 );

	try
	{
		std::vector<unsigned char> rawBuf;
		if (ExtractImageFromContainer(parms, rawBuf) != 0)
		{
			// エラー時は空文字を返却
			fmx::TextUniquePtr outText;
			results.SetAsText( *outText, results.GetLocale() );
			return 0;
		}

		int w = 0, h = 0, ch = 0;
		unsigned char* pixels = stbi_load_from_memory(rawBuf.data(), static_cast<int>(rawBuf.size()), &w, &h, &ch, 1); // 1 = grayscale
		if (!pixels)
		{
			SetLastError(2, "Failed to decompress image data (unsupported format or corrupted)");
			fmx::TextUniquePtr outText;
			results.SetAsText( *outText, results.GetLocale() );
			return 0;
		}

		// 巨大画像(50MP超)制限
		if (static_cast<long long>(w) * h > 50000000)
		{
			stbi_image_free(pixels);
			SetLastError(2, "Image dimensions are too large (exceeds 50 megapixels limit)");
			fmx::TextUniquePtr outText;
			results.SetAsText( *outText, results.GetLocale() );
			return 0;
		}

		ZXing::ImageView view(pixels, w, h, ZXing::ImageFormat::Lum);
		ZXing::ReaderOptions opts;
		opts.setTryHarder(true);
		opts.setTryRotate(true);
		opts.setTryInvert(true);

		auto barcodes = ZXing::ReadBarcodes(view, opts);
		stbi_image_free(pixels);

		fmx::TextUniquePtr outText;
		if (barcodes.empty())
		{
			SetLastError(3, "No barcode or QR code detected in the image");
		}
		else
		{
			std::string utf8Text = barcodes[0].text();
			outText->Assign( utf8Text.c_str(), fmx::Text::kEncoding_UTF8 );
		}

		results.SetAsText( *outText, results.GetLocale() );
		errorResult = 0;
	}
	catch (const std::exception& e)
	{
		SetLastError(9, std::string("Internal standard exception: ") + e.what());
		fmx::TextUniquePtr outText;
		results.SetAsText( *outText, results.GetLocale() );
		errorResult = 0;
	}
	catch (...)
	{
		SetLastError(9, "Internal unknown exception");
		fmx::TextUniquePtr outText;
		results.SetAsText( *outText, results.GetLocale() );
		errorResult = 0;
	}

	return errorResult;
}

// Do_QRB_DecodeAll ========================================================================
static FMX_PROC(fmx::errcode) Do_QRB_DecodeAll( short /* funcId */, const fmx::ExprEnv& /* environment */, const fmx::DataVect& parms, fmx::Data& results )
{
	SetLastError(0, ""); // エラー状態のクリア
	fmx::errcode errorResult( 960 );

	try
	{
		std::vector<unsigned char> rawBuf;
		if (ExtractImageFromContainer(parms, rawBuf) != 0)
		{
			fmx::TextUniquePtr outText;
			outText->Assign( "[]", fmx::Text::kEncoding_UTF8 );
			results.SetAsText( *outText, results.GetLocale() );
			return 0;
		}

		int w = 0, h = 0, ch = 0;
		unsigned char* pixels = stbi_load_from_memory(rawBuf.data(), static_cast<int>(rawBuf.size()), &w, &h, &ch, 1);
		if (!pixels)
		{
			SetLastError(2, "Failed to decompress image data (unsupported format or corrupted)");
			fmx::TextUniquePtr outText;
			outText->Assign( "[]", fmx::Text::kEncoding_UTF8 );
			results.SetAsText( *outText, results.GetLocale() );
			return 0;
		}

		if (static_cast<long long>(w) * h > 50000000)
		{
			stbi_image_free(pixels);
			SetLastError(2, "Image dimensions are too large (exceeds 50 megapixels limit)");
			fmx::TextUniquePtr outText;
			outText->Assign( "[]", fmx::Text::kEncoding_UTF8 );
			results.SetAsText( *outText, results.GetLocale() );
			return 0;
		}

		ZXing::ImageView view(pixels, w, h, ZXing::ImageFormat::Lum);
		ZXing::ReaderOptions opts;
		opts.setTryHarder(true);
		opts.setTryRotate(true);
		opts.setTryInvert(true);

		auto barcodes = ZXing::ReadBarcodes(view, opts);
		stbi_image_free(pixels);

		nlohmann::json jArr = nlohmann::json::array();
		fmx::TextUniquePtr outText;

		if (barcodes.empty())
		{
			SetLastError(3, "No barcode or QR code detected in the image");
		}
		else
		{
			for (const auto& b : barcodes)
			{
				nlohmann::json jItem;
				jItem["text"] = b.text();
				jItem["format"] = ZXing::ToString(b.format());
				jArr.push_back(jItem);
			}
		}

		std::string jsonStr = jArr.dump();
		outText->Assign( jsonStr.c_str(), fmx::Text::kEncoding_UTF8 );
		results.SetAsText( *outText, results.GetLocale() );
		errorResult = 0;
	}
	catch (const std::exception& e)
	{
		SetLastError(9, std::string("Internal standard exception: ") + e.what());
		fmx::TextUniquePtr outText;
		outText->Assign( "[]", fmx::Text::kEncoding_UTF8 );
		results.SetAsText( *outText, results.GetLocale() );
		errorResult = 0;
	}
	catch (...)
	{
		SetLastError(9, "Internal unknown exception");
		fmx::TextUniquePtr outText;
		outText->Assign( "[]", fmx::Text::kEncoding_UTF8 );
		results.SetAsText( *outText, results.GetLocale() );
		errorResult = 0;
	}

	return errorResult;
}

// Do_QRB_LastError ========================================================================
static FMX_PROC(fmx::errcode) Do_QRB_LastError( short /* funcId */, const fmx::ExprEnv& /* environment */, const fmx::DataVect& /* parms */, fmx::Data& results )
{
	fmx::errcode errorResult( 960 );

	try
	{
		fmx::TextUniquePtr outText;
		outText->Assign( g_lastError.c_str(), fmx::Text::kEncoding_UTF8 );
		results.SetAsText( *outText, results.GetLocale() );
		errorResult = 0;
	}
	catch (...)
	{
		errorResult = 960;
	}

	return errorResult;
}

// Do_PluginInit ===========================================================================
static fmx::ptrtype Do_PluginInit( fmx::int16 version )
{
	fmx::ptrtype                    result( static_cast<fmx::ptrtype>(kDoNotEnable) );
	const fmx::QuadCharUniquePtr    pluginID( kQRB_PluginID[0], kQRB_PluginID[1], kQRB_PluginID[2], kQRB_PluginID[3] );
	fmx::TextUniquePtr              name;
	fmx::TextUniquePtr              definition;
	fmx::TextUniquePtr              description;
	fmx::uint32                     flags( fmx::ExprEnv::kDisplayInAllDialogs | fmx::ExprEnv::kFutureCompatible );

	if (version >= k150ExtnVersion)
	{
		// 100: QRB_Version
		name->Assign( kQRB_VersionName, fmx::Text::kEncoding_UTF8 );
		definition->Assign( kQRB_VersionDefinition, fmx::Text::kEncoding_UTF8 );
		description->Assign( kQRB_VersionDescription, fmx::Text::kEncoding_UTF8 );
		if (fmx::ExprEnv::RegisterExternalFunctionEx( *pluginID, kQRB_VersionID, *name, *definition, *description, kQRB_VersionMin, kQRB_VersionMax, flags, Do_QRB_Version ) == 0)
		{
			// 101: QRB_Decode
			name->Assign( kQRB_DecodeName, fmx::Text::kEncoding_UTF8 );
			definition->Assign( kQRB_DecodeDefinition, fmx::Text::kEncoding_UTF8 );
			description->Assign( kQRB_DecodeDescription, fmx::Text::kEncoding_UTF8 );
			if (fmx::ExprEnv::RegisterExternalFunctionEx( *pluginID, kQRB_DecodeID, *name, *definition, *description, kQRB_DecodeMin, kQRB_DecodeMax, flags, Do_QRB_Decode ) == 0)
			{
				// 102: QRB_DecodeAll
				name->Assign( kQRB_DecodeAllName, fmx::Text::kEncoding_UTF8 );
				definition->Assign( kQRB_DecodeAllDefinition, fmx::Text::kEncoding_UTF8 );
				description->Assign( kQRB_DecodeAllDescription, fmx::Text::kEncoding_UTF8 );
				if (fmx::ExprEnv::RegisterExternalFunctionEx( *pluginID, kQRB_DecodeAllID, *name, *definition, *description, kQRB_DecodeAllMin, kQRB_DecodeAllMax, flags, Do_QRB_DecodeAll ) == 0)
				{
					// 103: QRB_LastError
					name->Assign( kQRB_LastErrorName, fmx::Text::kEncoding_UTF8 );
					definition->Assign( kQRB_LastErrorDefinition, fmx::Text::kEncoding_UTF8 );
					description->Assign( kQRB_LastErrorDescription, fmx::Text::kEncoding_UTF8 );
					if (fmx::ExprEnv::RegisterExternalFunctionEx( *pluginID, kQRB_LastErrorID, *name, *definition, *description, kQRB_LastErrorMin, kQRB_LastErrorMax, flags, Do_QRB_LastError ) == 0)
					{
						result = kCurrentExtnVersion;
					}
				}
			}
		}
	}

	return result;
}

// Do_PluginShutdown =======================================================================
static void Do_PluginShutdown( fmx::int16 version )
{
	const fmx::QuadCharUniquePtr    pluginID( kQRB_PluginID[0], kQRB_PluginID[1], kQRB_PluginID[2], kQRB_PluginID[3] );

	if (version >= k150ExtnVersion)
	{
		static_cast<void>(fmx::ExprEnv::UnRegisterExternalFunction( *pluginID, kQRB_VersionID ));
		static_cast<void>(fmx::ExprEnv::UnRegisterExternalFunction( *pluginID, kQRB_DecodeID ));
		static_cast<void>(fmx::ExprEnv::UnRegisterExternalFunction( *pluginID, kQRB_DecodeAllID ));
		static_cast<void>(fmx::ExprEnv::UnRegisterExternalFunction( *pluginID, kQRB_LastErrorID ));
	}
}

// Do_GetString ============================================================================
static void CopyUTF8StrToUnichar16Str( const char* inStr, fmx::uint32 outStrSize, fmx::unichar16* outStr )
{
	if (outStrSize == 0)
	{
		return;
	}
	fmx::TextUniquePtr txt;
	txt->Assign( inStr, fmx::Text::kEncoding_UTF8 );
	const fmx::uint32 txtSize( (outStrSize <= txt->GetSize()) ? (outStrSize - 1) : txt->GetSize() );
	txt->GetUnicode( outStr, 0, txtSize );
	outStr[txtSize] = 0;
}

static void Do_GetString( fmx::uint32 whichString, fmx::uint32 /* winLangID */, fmx::uint32 outBufferSize, fmx::unichar16* outBuffer )
{
	switch (whichString)
	{
		case kFMXT_NameStr:
		{
			CopyUTF8StrToUnichar16Str( kQRB_PluginName, outBufferSize, outBuffer );
			break;
		}

		case kFMXT_AppConfigStr:
		{
			CopyUTF8StrToUnichar16Str( kQRB_PluginDescription, outBufferSize, outBuffer );
			break;
		}

		case kFMXT_OptionsStr:
		{
			// 1〜4 文字目: プラグイン ID
			CopyUTF8StrToUnichar16Str( kQRB_PluginID, outBufferSize, outBuffer );

			// 5 文字目: 常に "1"
			outBuffer[4] = '1';

			// 6 文字目: 環境設定の Configure ボタン(不要なので n)
			outBuffer[5] = 'n';

			// 7 文字目: 常に "n"
			outBuffer[6] = 'n';

			// 8 文字目: kFMXT_Init / kFMXT_Shutdown を受け取る
			outBuffer[7] = 'Y';

			// 9 文字目: kFMXT_Idle(v1 は同期処理のみなので不要)
			outBuffer[8] = 'n';

			// 10 文字目: kFMXT_SessionShutdown / kFMXT_FileShutdown
			outBuffer[9] = 'n';

			// 11 文字目: 常に "n"
			outBuffer[10] = 'n';

			outBuffer[11] = 0;
			break;
		}

		default:
		{
			outBuffer[0] = 0;
			break;
		}
	}
}

// FMExternCallProc ========================================================================
FMX_ExternCallPtr gFMX_ExternCallPtr( nullptr );

void FMX_ENTRYPT FMExternCallProc( FMX_ExternCallPtr pb )
{
	gFMX_ExternCallPtr = pb;

	switch (pb->whichCall)
	{
		case kFMXT_Init:
			pb->result = Do_PluginInit( pb->extnVersion );
			break;

		case kFMXT_Shutdown:
			Do_PluginShutdown( pb->extnVersion );
			break;

		case kFMXT_GetString:
			Do_GetString( static_cast<fmx::uint32>(pb->parm1), static_cast<fmx::uint32>(pb->parm2), static_cast<fmx::uint32>(pb->parm3), reinterpret_cast<fmx::unichar16*>(pb->result) );
			break;
	}
}
