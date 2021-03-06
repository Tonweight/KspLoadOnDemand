#pragma once


using namespace System;
using namespace System::Runtime::InteropServices;

#include "KSPThread.h"
#include "Work.h"
#include "ITexture.h"
#include "TextureManager.h"
#include "DirectXStuff.h"

namespace LodNative{
	// helper class for GPU related stuff


	/*
		Texture creation process:
		- Go to GPU thread
		- ask Assignable for texture format
		- try to create this texture, though with adjustments made by D3DX_CheckTextureRequirementsToThrow
		- Where there any adjustments?
		  - Inform 

	*/
	ref class GPU{
	private:
		ref class CreateTextureForAssignableAsyncScope{
		private:
			AssignableFormat format;
			IAssignableTexture^ assignable;
			Action<IntPtr>^ callback;
		public:
			CreateTextureForAssignableAsyncScope(IAssignableTexture^ assignableTexture, Action<IntPtr>^ onSuccessCallback): 
				format(assignableTexture->GetAssignableFormat()), 
				assignable(assignableTexture), 
				callback(onSuccessCallback){
			}
			void RunWithFormat(IDirect3DDevice9* device){
				auto texture = DirectXStuff::Device_CreateTextureOrThrow(device, format.Width, format.Height, format.Levels, format.Levels == 0 ? D3DUSAGE_AUTOGENMIPMAP : 0, format.Format, D3DPOOL::D3DPOOL_MANAGED);
				try{


					D3DLOCKED_RECT lockedRect;
					DirectXStuff::Texture_LockRectOrThrow(texture, 0, &lockedRect, NULL, 0);
					try{
						assignable->AssignToTarget(&lockedRect);
						//old: assignableData->AssignTo(gcnew AssignableTarget(format, &lockedRect, true /*Unity textures seem to be upside down...*/));
						Logger::LogText("Assigned texture " + assignable->DebugInfo->ToString());
					}
					finally{
						DirectXStuff::Texture_UnlockRectOrThrow(texture, 0);
					}
					// Todo25: Check whether PreLoad has any (positive) effect
					texture->PreLoad();


					callback(IntPtr(texture));
					delete assignable;
					texture = NULL;
				}
				finally{
					if (texture != NULL){
						texture->Release();
					}
				}

			}
			void RunAfterConversion(IntPtr devicePtr){
				Logger::LogText("Retrying GpuCopy post-convert of " + assignable->DebugInfo);
				RunWithFormat((IDirect3DDevice9*)devicePtr.ToPointer());
			}
			void RunAssignableConversion(){
				Logger::LogText("PreGpuCopy-converting Texture " + assignable->DebugInfo + " to " + format.ToString());
				assignable = assignable->ConvertToAssignableFormat(format);
			}
			bool CheckTextureRequirementsForFormat(IDirect3DDevice9* device, AssignableFormat% fmt){
				UINT w = fmt.Width,
					h = fmt.Height,
					l = fmt.Levels;
				D3DFORMAT f = fmt.Format;
				DirectXStuff::D3DX_CheckTextureRequirementsToThrow(device, &w, &h, &l, 0, &f, D3DPOOL::D3DPOOL_MANAGED);
				if (w == fmt.Width && h == fmt.Height && f == fmt.Format){
					if ((l == fmt.Levels) || (fmt.Levels == 0)){
						return true;
					}
				}
				Logger::LogText("DX Forces format change! " + fmt.ToString() + " to " + AssignableFormat(w, h, l, f).ToString());
				fmt.Width = w;
				fmt.Height = h;
				fmt.Levels = l;
				fmt.Format = f;
				return false;
			}
			void Run(IntPtr devicePtr){
				Logger::LogText("Trying GpuCopy of " + assignable->DebugInfo);
				auto device = (IDirect3DDevice9*)devicePtr.ToPointer();
				if (CheckTextureRequirementsForFormat(device, format)){
					RunWithFormat(device);
				}
				else{
					Work::SchedulePriority(gcnew Action(this, &CreateTextureForAssignableAsyncScope::RunAssignableConversion));
				}
			}
		};
	public:

		static void CreateTextureForAssignable(IAssignableTexture^ assignableTexture, Action<IntPtr>^ onSuccessCallback){
			KSPThread::EnqueueJob(gcnew Action<IntPtr>(gcnew CreateTextureForAssignableAsyncScope(assignableTexture, onSuccessCallback), &CreateTextureForAssignableAsyncScope::Run));
		}
	};

	// An old and "more complex" version...
#if old
	ref class GPU{
	public:
		static void CopyBuffer(unsigned char* src_buffer, int src_pitch, unsigned char* trg_buffer, int trg_pitch, int line_size_in_bytes, int height){

			if (src_pitch == trg_pitch && src_pitch == line_size_in_bytes){
				memcpy(trg_buffer, src_buffer, line_size_in_bytes*height);
			}
			else{
				for (int y = 0; y < height; y++){
					memcpy(trg_buffer, src_buffer, line_size_in_bytes);
					trg_buffer += trg_pitch;
					src_buffer += src_pitch;
				}
			}
		}
		// Unity textures seem to be upside down... we should be able to handle it easily, though...
		// https://developer.oculusvr.com/forums/viewtopic.php?f=37&t=896
		static void CopyBufferInverted(unsigned char* src_buffer, int src_pitch, unsigned char* trg_buffer, int trg_pitch, int line_size_in_bytes, int height){

			CopyBuffer(src_buffer + ((height - 1)*src_pitch), -src_pitch, trg_buffer, trg_pitch, line_size_in_bytes, height);
		}
		/*
		static String^ Debug_FristBytesToText(unsigned char* data, int bytes){
		auto sb = gcnew System::Text::StringBuilder();
		bool isFirst = true;
		for (int x = 0; x < bytes; x++){
		if (isFirst){
		isFirst = false;
		}
		else{
		sb->Append(",");
		}
		sb->Append(*data);
		data++;
		}
		return sb->ToString();
		}*/
		static void CopyRawBytesTo(AssignableTarget^ trg, unsigned char* src_buffer, int src_pitch, int line_size_in_bytes, int height, bool srcIsUpsideDown, TextureDebugInfo^ info){

			/*
			auto sb = gcnew System::Text::StringBuilder();
			unsigned char* srcPtr = src_buffer;
			for (int x = src_pitch * height; x > 0; x--){
			sb->AppendLine((*srcPtr).ToString());
			srcPtr++;
			}
			System::IO::File::WriteAllText(info->File + "_DUMP1.txt", sb->ToString());

			sb = gcnew System::Text::StringBuilder();
			if (trg->Inverted != srcIsUpsideDown){
			unsigned char* linePtr = trg->Bits + ((height - 1)*trg->Pitch);
			for (int y = 0; y < height; y++){
			unsigned char* pixelPtr = linePtr;
			for (int x = 0; x < trg->Pitch; x++){
			sb->AppendLine((*pixelPtr).ToString());
			pixelPtr++;
			}
			linePtr = linePtr - trg->Pitch;
			}
			}
			else{
			unsigned char* trgPtr = trg->Bits;
			for (int x = trg->Pitch * height; x > 0; x--){
			sb->AppendLine((*trgPtr).ToString());
			trgPtr++;
			}
			}
			System::IO::File::WriteAllText(info->File + "_DUMP2.txt", sb->ToString());*/
			/*
			for (int x = 0; x < 50; x++){
			sb->AppendLine();
			}*/

			//Logger::LogText(sb->ToString());
			int line_size = trg->Format->BytesPerPixel * trg->Format->Width;
			if (
				(height > trg->Format->Height)
				|| (line_size < src_pitch)
				|| (line_size < trg->Pitch)
				){
				throw gcnew ArgumentException();
			}

			/*

			//auto file = "C:\\ksp_rtLodNew\\" + "LOD_DEBUG_" + ->Replace(":", "")->Replace(" ", "") + ".png";

			Logger::LogText(
			Environment::NewLine
			+ Environment::NewLine + "  Copying to GPU: " + info->File + info->Modifiers
			+ Environment::NewLine + "  Saving img copy to " + info->LogFileBaseName);
			auto bmp = gcnew Drawing::Bitmap(trg->Format->Width, trg->Format->Height, AssignableFormat::PixelFormatFromD3DFormat(trg->Format->Format));
			auto bmpData = bmp->LockBits(Drawing::Rectangle(0, 0, bmp->Width, bmp->Height), ImageLockMode::WriteOnly, bmp->PixelFormat);
			auto sourceBytes = Debug_FristBytesToText(src_buffer, 50);
			CopyBuffer(src_buffer, src_pitch, (unsigned char*)bmpData->Scan0.ToPointer(), bmpData->Stride, line_size, bmp->Height);
			bmp->UnlockBits(bmpData);
			bmp->Save(info->LogFileBaseName + "_OUT.png", ImageFormat::Png);
			System::IO::File::Copy(info->File, info->LogFileBaseName + "_SOURCE" + ((gcnew System::IO::FileInfo(info->File))->Extension));
			auto trgBmp = gcnew Drawing::Bitmap(trg->Format->Width, trg->Format->Height, AssignableFormat::PixelFormatFromD3DFormat(trg->Format->Format));
			auto trgData = trgBmp->LockBits(Drawing::Rectangle(0, 0, bmp->Width, bmp->Height), ImageLockMode::WriteOnly, bmp->PixelFormat);
			auto targetBytes = Debug_FristBytesToText((unsigned char*)trgData->Scan0.ToPointer(), 50);
			CopyBuffer(trg->Bits, trg->Pitch, (unsigned char*)trgData->Scan0.ToPointer(), trgData->Stride, line_size, bmp->Height);
			trgBmp->UnlockBits(trgData);
			trgBmp->Save(info->LogFileBaseName + "_TARGET.png", ImageFormat::Png);
			*/
			if (trg->Inverted != srcIsUpsideDown){
				CopyBufferInverted(src_buffer, src_pitch, trg->Bits, trg->Pitch, line_size, height);
			}
			else{
				CopyBuffer(src_buffer, src_pitch, trg->Bits, trg->Pitch, line_size, height);
			}
			/*
			auto writtenBmp = gcnew Drawing::Bitmap(trg->Format->Width, trg->Format->Height, AssignableFormat::PixelFormatFromD3DFormat(trg->Format->Format));
			auto writtenData = writtenBmp->LockBits(Drawing::Rectangle(0, 0, bmp->Width, bmp->Height), ImageLockMode::WriteOnly, bmp->PixelFormat);
			auto writtenBytes = Debug_FristBytesToText((unsigned char*)trg->Bits, 50);
			CopyBuffer(trg->Bits, trg->Pitch, (unsigned char*)writtenData->Scan0.ToPointer(), writtenData->Stride, line_size, bmp->Height);
			writtenBmp->UnlockBits(writtenData);
			writtenBmp->Save(info->LogFileBaseName + "_WRITTE.png", ImageFormat::Png);


			Logger::LogText("Content comparison:"
			+ Environment::NewLine + "SRC: " + sourceBytes
			+ Environment::NewLine + "TRG: " + targetBytes
			+ Environment::NewLine + "WRT: " + writtenBytes
			);
			*/
		}


	private:
		static void CopyToTexture(IDirect3DTexture9* texture, AssignableData^ assignableData, AssignableFormat^ format){
			D3DLOCKED_RECT lockedRect;
			DirectXStuff::Texture_LockRectOrThrow(texture, 0, &lockedRect, NULL, 0);
			try{
				assignableData->AssignTo(gcnew AssignableTarget(format, &lockedRect, true /*Unity textures seem to be upside down...*/));
				Logger::LogText("Assigned texture " + assignableData->Debug);
			}
			finally{
				DirectXStuff::Texture_UnlockRectOrThrow(texture, 0);
			}
			// Todo25: Check whether PreLoad has a (positive) effect
			texture->PreLoad();
		}
		ref class CreateForDataTextureAsyncScope{
		private:
			Func<AssignableFormat^, AssignableData^>^ delayedCallbackToRun;
			AssignableFormat^ acceptedFormat;
			AssignableData^ delayedAssignableData;
		public:
			IAssignableTexture^ assignableTexture;
			int textureId;
			CreateForDataTextureAsyncScope(IAssignableTexture^ assignable_texture, int texture_id){
				assignableTexture = assignable_texture;
				textureId = texture_id;
			}
			void RunWithAssignable(IDirect3DDevice9* device, AssignableFormat^ format, AssignableData^ data){
				IDirect3DTexture9* dxTexture = DirectXStuff::Device_CreateTextureOrThrow(device, format->Width, format->Height, 0, D3DUSAGE_AUTOGENMIPMAP, format->Format, D3DPOOL::D3DPOOL_MANAGED);
				try{
					CopyToTexture(dxTexture, data, format);
				}
				catch (Exception^){
					dxTexture->Release();
					throw;
				}
				TextureManager::OnTextureLoadedToGPU(dxTexture, textureId);
			}
			void RunDelayedSuccessful(IntPtr devicePtr){
				RunWithAssignable((IDirect3DDevice9*)devicePtr.ToPointer(), acceptedFormat, delayedAssignableData);
			}
			void RunDelayedCallback(){

				auto ad = delayedCallbackToRun(acceptedFormat);
				if (ad == nullptr){
					throw gcnew InvalidOperationException("Texture did not return assignable data.");
				}
				else{
					KSPThread::EnqueueJob(gcnew Action<IntPtr>(this, &CreateForDataTextureAsyncScope::RunDelayedSuccessful));
				}

			}
			void Run(IntPtr devicePtr){
				auto device = (IDirect3DDevice9*)devicePtr.ToPointer();
				/*auto bmpFmt = dynamic_cast<BitmapFormat^>(assignableTexture);
				if (bmpFmt != nullptr){
					bmpFmt->Bitmap->Save(bmpFmt->DebugInfo->TexFile + ".png.assign", Imaging::ImageFormat::Png);
					auto tw = System::IO::File::CreateText(bmpFmt->DebugInfo->TexFile + ".assign.txt");

					for (int y = 0; y < bmpFmt->Bitmap->Height; y++)
					{
						for (int x = 0; x < bmpFmt->Bitmap->Width; x++)
						{
							auto c = bmpFmt->Bitmap->GetPixel(x, y);
							tw->Write("{" + c.A + "|" + c.R + "|" + c.G + "|" + c.B + "}");
						}
						tw->WriteLine();
					}
					tw->Close();
					delete tw;
				}*/
				auto format = assignableTexture->GetAssignableFormat();
				UINT width = format->Width, height = format->Height;
				D3DFORMAT texFormat = format->Format;
				DirectXStuff::D3DX_CheckTextureRequirementsToThrow(device, &width, &height, 0, 0, &texFormat, D3DPOOL::D3DPOOL_MANAGED);
				auto changedFormat = gcnew AssignableFormat(width, height, texFormat);
				format = changedFormat->SameAs(format) ? nullptr : changedFormat;
				Func<AssignableFormat^, AssignableData^>^ delayedCallback;
				auto assignableData = assignableTexture->GetAssignableData(format, delayedCallback);
				if (assignableData == nullptr){
					if (delayedCallback == nullptr){
						throw gcnew InvalidOperationException("Texture did not return assignable data.");
					}
					else{
						delayedCallbackToRun = delayedCallback;
						acceptedFormat = changedFormat;
						Work::SchedulePriority(gcnew Action(this, &CreateHighResTextureAsyncScope::RunDelayedCallback));
					}
				}
				else{
					RunWithAssignable(device, changedFormat, assignableData);
				}
			}
		};
	public:
		static void CreateForDataTextureAsync(IAssignableTexture^ self, int texture_id){
			KSPThread::EnqueueJob(gcnew Action<IntPtr>(gcnew CreateForDataTextureAsyncScope(self, texture_id), &CreateForDataTextureAsyncScope::Run));
		}
	private:
		ref class AssignDataToExistingTextureAsyncScope{
		public:
			int textureId;
			IAssignableTexture^ assignableTexture;
			IDirect3DTexture9* targetTexture;
			AssignDataToExistingTextureAsyncScope(IAssignableTexture^ assignable, IDirect3DTexture9* target, int textureId){
				assignableTexture = assignable;
				targetTexture = target;
				this->textureId = textureId;
			}
			void Run(IntPtr devicePtr){
				D3DSURFACE_DESC texDesc;
				targetTexture->GetLevelDesc(0, &texDesc);
				auto format = gcnew AssignableFormat(texDesc.Width, texDesc.Height, texDesc.Format);
				Func<AssignableFormat^, AssignableData^>^ delayedCallback;
				auto assignableData = assignableTexture->GetAssignableData(
					format->SameAs(assignableTexture->GetAssignableFormat()) ? nullptr : format,
					delayedCallback);
				if (assignableData == nullptr){
					if (delayedCallback == nullptr){
						throw gcnew InvalidOperationException("Texture did not return assignable data.");
					}
					else{
						throw gcnew NotImplementedException("Not sure whether i want support that for AssignToTexture");
					}
				}
				else{
					CopyToTexture(targetTexture, assignableData, format);
					delete assignableTexture;
					ManagedBridge::ThumbnailUpdated(textureId);
					//Logger::LogText("success reporting?");
				}
			}
		};
	public:
		static void AssignDataToExistingTextureAsync(IAssignableTexture^ self, IDirect3DTexture9* texture, Action^ callback/* int textureId */){
			KSPThread::EnqueueJob(gcnew Action<IntPtr>(gcnew AssignDataToExistingTextureAsyncScope(self, texture, textureId), &AssignDataToExistingTextureAsyncScope::Run));
		}

	};
#endif
}