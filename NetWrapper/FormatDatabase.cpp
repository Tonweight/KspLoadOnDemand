#include "Stdafx.h"
#include "FormatDatabase.h"


#include "CachedTexture.h"
#include "MBMTexture.h"
#include "TargaImage.h"

using namespace LodNative;

BitmapFormat^ FormatDatabase::TryRecognizeTGA(FileInfo^ file, BufferMemory::ISegment^ data, int textureId){
	if (file->Extension->ToUpper() == ".TGA"){
		Paloma::TargaImage^ img = nullptr;
		System::IO::MemoryStream^ ms = nullptr;
		try{
			Logger::LogText("Loading " + file->FullName);
			ms = data->CreateStream();
			auto img = gcnew Paloma::TargaImage();
			img->LoadTGAFromMemory(ms);


			//img->Image->Save(file->FullName + ".png", System::Drawing::Imaging::ImageFormat::Png);

			auto bmp = (gcnew BitmapFormat(gcnew Bitmap(img->Image), false, gcnew TextureDebugInfo(file->FullName, textureId)))->MayToNormal(BitmapFormat::FileNameIndicatesTextureShouldBeNormal(file));
			//bmp->Bitmap->Save(sFileName + "2.png", System::Drawing::Imaging::ImageFormat::Png);
			delete img;
			delete ms;
			data->Free();
			bmp->FlipVertical();
			return bmp;
			// todo: find out whether this has any effect (on KSP/pinvoke aswell!)
			System::GC::Collect();
			System::GC::WaitForFullGCComplete();
		}
#if NDEBUG
		catch (Exception^ err){
			throw gcnew Exception(
				"Failed to load TGA file: " + file->FullName
				+ (img != nullptr ? (img->Image != nullptr ? (", " + img->Image->Width + "x" + img->Image->Height + "@" + img->Image->PixelFormat.ToString()) : "") : "")
				, err);
		}
#endif
		finally{}
		//return gcnew BitmapFormat(gcnew Bitmap(16, 16, PixelFormat::Format32bppArgb), false, gcnew TextureDebugInfo("FakeTGA"));
	}
	return nullptr;
}
static FormatDatabase::FormatDatabase(){
	AddRecognition(gcnew Func<FileInfo^, BufferMemory::ISegment^, int, MBMTexture^>(&MBMTexture::TryRecognizeFile));
	AddRecognition(gcnew Func<FileInfo^, BufferMemory::ISegment^, int, CachedTexture^>(&CachedTexture::TryRecognizeFile));
	AddRecognition(gcnew Func<FileInfo^, BufferMemory::ISegment^, int, BitmapFormat^>(&FormatDatabase::TryRecognizeTGA));
	AddConversion(gcnew Func<CachedTexture^, BitmapFormat^>(&CachedTexture::ConvertToBitmap));
	AddConversion(gcnew Func<MBMTexture^, BitmapFormat^>(&MBMTexture::ConvertToBitmap));

}