﻿using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using UnityEngine;

namespace LoadOnDemand
{
    static class ConfigExtensions
    {
        public static T GetValue<T>(this ConfigNode self, string name, Func<string, T> converter, T defaultValue)
        {
            return self.HasValue(name) ? converter(self.GetValue(name)) : defaultValue;
        }
    }
    class ImageSettings
    {

        public bool? ThumbnailEnabled { get; private set; }
        public int? ThumbnailWidth { get; private set; }
        public int? ThumbnailHeight { get; private set; }
        //public TextureFormat ThumbnailFormat { get; private set; }
        public bool? ThumbnailCompress { get; private set; }

        public bool? HighResEnabled { get; private set; }
        public bool? HighResCompress { get; private set; }


        public static ImageSettings CreateDefaults()
        {
            return new ImageSettings()
            {
                ThumbnailEnabled = true,
                ThumbnailWidth = 32,
                ThumbnailHeight = 32,
                ThumbnailCompress = false,
                HighResEnabled = true,
                HighResCompress = false
            };
        }

        public void WriteToConfigNode(ConfigNode node)
        {
            if(ThumbnailEnabled.HasValue)
                node.AddValue("ThumbnailEnabled", ThumbnailEnabled.Value.ToString());
            if(ThumbnailWidth.HasValue)
                node.AddValue("ThumbnailWidth", ThumbnailWidth.Value.ToString());
            if(ThumbnailHeight.HasValue)
                node.AddValue("ThumbnailHeight", ThumbnailHeight.Value.ToString());
            if(ThumbnailCompress.HasValue)
                node.AddValue("ThumbnailCompress", ThumbnailCompress.Value.ToString());
            if (HighResEnabled.HasValue)
                node.AddValue("HighResEnabled", HighResEnabled.Value.ToString());
            if (HighResCompress.HasValue)
                node.AddValue("HighResCompress", HighResCompress.Value.ToString());
        }
        public static ImageSettings FromConfigNode(ConfigNode node)
        {
            return new ImageSettings()
            {
                ThumbnailEnabled = node.GetValue<bool?>("ThumbnailEnabled", s => bool.Parse(s), null),
                ThumbnailWidth = node.GetValue<int?>("ThumbnailWidth", s => int.Parse(s), null),
                ThumbnailHeight = node.GetValue<int?>("ThumbnailHeight", s => int.Parse(s), null),
                ThumbnailCompress = node.GetValue<bool?>("ThumbnailCompress", s => bool.Parse(s), null),
                HighResEnabled = node.GetValue<bool?>("HighResEnabled", s => bool.Parse(s), null),
                HighResCompress = node.GetValue<bool?>("HighResCompress", s => bool.Parse(s), null)
            };
        }
        public static ImageSettings Combine(ImageSettings first, ImageSettings second)
        {
            var result = new ImageSettings()
            {
                ThumbnailEnabled = first.ThumbnailEnabled.HasValue ? first.ThumbnailEnabled : second.ThumbnailEnabled,
                ThumbnailWidth = first.ThumbnailWidth.HasValue ? first.ThumbnailWidth : second.ThumbnailWidth,
                ThumbnailHeight = first.ThumbnailHeight.HasValue ? first.ThumbnailHeight : second.ThumbnailHeight,
                ThumbnailCompress = first.ThumbnailCompress.HasValue ? first.ThumbnailCompress : second.ThumbnailCompress,
                HighResEnabled = first.HighResEnabled.HasValue ? first.HighResEnabled : second.HighResEnabled,
                HighResCompress = first.HighResCompress.HasValue ? first.HighResCompress : second.HighResCompress
            };
            /*("Combining: {"
                + Environment.NewLine + "  ThumbnailEnabled: " + first.ThumbnailEnabled + "#" + second.ThumbnailEnabled + "=>" + result.ThumbnailEnabled + ","
                + Environment.NewLine + "  ThumbnailWidth: " + first.ThumbnailWidth + "#" + second.ThumbnailWidth + "=>" + result.ThumbnailWidth + ","
                + Environment.NewLine + "  ThumbnailHeight: " + first.ThumbnailHeight + "#" + second.ThumbnailHeight + "=>" + result.ThumbnailHeight + ","
                + Environment.NewLine + "  ThumbnailCompress: " + first.ThumbnailCompress + "#" + second.ThumbnailCompress + "=>" + result.ThumbnailCompress + ","
                + Environment.NewLine + "  HighResCompress: " + first.HighResCompress + "#" + second.HighResCompress + "=>" + result.HighResCompress 
                + Environment.NewLine + "}").Log();*/
            return result;
        }
    }
    class ImageConfigItem
    {
        private static string escapeIdsSpaces(string idWithSpaces)
        {
            // shitty cfg nodes can't handle lot of chars... this "escapes" them
            return idWithSpaces.Replace(@"\", @"\\").Replace(" ", @"\s").Replace("(", @"\i").Replace(")", @"\o");
        }
        private static string unescapeIdsSpaces(string escapedId)
        {
            // shitty cfg nodes can't handle lot of chars... this "unescapes" them
            return escapedId.Replace( @"\o", ")").Replace(@"\i", "(").Replace(@"\s", " ").Replace(@"\\", @"\");
        }

        public string FileUrl { get; private set; }
        public string CacheKey { get; private set; }
        public long FileSize { get; private set; }
        public DateTime LastChanged { get; private set; }
        public bool SkipImage { get; private set; }
        public ImageSettings ImageSettings { get; private set; }

        public bool isCacheValid(UrlDir.UrlFile file)
        {
            if ((file.fileTime - LastChanged).TotalMinutes > 1)
            {
                ("Cache miss: " + FileUrl + "; Date [" + file.fileTime.ToString() + "(aka " + file.fileTime.Ticks + ") vs " + LastChanged.ToString() + " (aka " + LastChanged.Ticks + "]").Log();
            }
            else if (new System.IO.FileInfo(file.fullPath).Length != FileSize)
            {
                ("Cache miss (size): " + FileUrl).Log();
            }
            else
            {
                return true;
            }
            return false;
        }
        public void updateVerificationData(UrlDir.UrlFile file)
        {
            LastChanged = file.fileTime;
            FileSize = new System.IO.FileInfo(file.fullPath).Length;
        }
        public void clearCache(string directory)
        {
            var di = new System.IO.DirectoryInfo(directory);
            foreach (var oldFile in di.GetFiles(CacheKey + ".*"))
            {
                ("Deleting old cache file: " + oldFile.FullName).Log();
                oldFile.Delete();
            }
        }
        public ImageConfigItem()
        {
            ImageSettings = new ImageSettings();
        }
        public ImageConfigItem(string cacheKey, UrlDir.UrlFile fromFile)
        {
            ImageSettings = new ImageSettings();
            CacheKey = cacheKey;
            FileUrl = fromFile.url + "."+ fromFile.fileExtension;
            updateVerificationData(fromFile);
        }
        public static ImageConfigItem FromConfigNode(ConfigNode node)
        {
            if (node.HasValue("SkipImage"))
            {
                bool skipImage;
                if (bool.TryParse(node.GetValue("SkipImage"), out skipImage))
                {
                    if (skipImage)
                    {
                        return new ImageConfigItem() { 
                            SkipImage = true, 
                            FileUrl = unescapeIdsSpaces(node.name)
                        };
                    }
                }
                else
                {
                    ("Warning: Invalid SkipImage value [" + node.GetValue("SkipImage") + "] was ignored").Log();
                }
            }
            return new ImageConfigItem()
            {
                FileUrl = unescapeIdsSpaces(node.name),
                CacheKey = node.GetValue("Key"),
                //Url = node.GetValue("Url"),
                FileSize = long.Parse(node.GetValue("Size")),
                LastChanged = DateTime.Parse(node.GetValue("Date")),
                ImageSettings = ImageSettings.FromConfigNode(node)
            };
        }
        public ConfigNode ToConfigNode()
        {
            var node = new ConfigNode(escapeIdsSpaces(FileUrl));
            if (SkipImage)
            {
                node.AddValue("SkipImage", true.ToString());
            }
            else
            {
                node.AddValue("Key", CacheKey);
                //node.AddValue("Url", el.Value.Url);
                node.AddValue("Size", FileSize.ToString());
                node.AddValue("Date", LastChanged.ToString());
            }
            return node;
        }
    }
    class Config
    {
        public static bool Disabled { get; set; }

        public static Config Current { get; private set; }
        System.IO.FileInfo cfgFileLocation;
        private Config() {
            // defaults:

            DefaultImageSettings = ImageSettings.CreateDefaults();

            UI_DelayBeforeHidingActivityUI = TimeSpan.FromSeconds(3);
            UI_DelayBeforeShowingActivityUI = TimeSpan.FromSeconds(3);

            UI_TryUseToolbarForDebugUI = true;
            UI_DisplayDebugUI = false;
            Debug_DontLoadEditorCatalogThumbnailParts = false; // notimplementedexception, debug, todo: default to false once we got DXT!

        }
        /*
        * 
        * LOD_CONFIG
        * 
        *      Thumb
         *         Enabled = true // turn of thumb usage... though no fallback for editor!
        *          Width  = 16
        *          Height = 16
        *          Format = RGBA32 // dont rly think others make sense, though...
        *          
        *      CompressTextures = true // Compress images to DXT1/5... creates 
        *      
        *      Cache
         *          key
        *               Url
        *               Size
        *               Date
        *               // Original_Hash?
        *               // ProcessedDataIsNormal
        *               // CacheName (no path, no extension, can be used for both thumb and)
        *          
        *      // SPECIAL_IMG
        *          // kinda same as IMG, but for embeded images... we cannot improve loading times here, 
        *          // but could reduce or compress them... caching might be an idea, but a whitelist seems necessary?
        * 
        * 
        */
        static Config()
        {
            Current = new Config();
            var gameData = System.IO.Path.Combine(UrlDir.ApplicationRootPath, "GameData");
            var lodDir = System.IO.Path.Combine(gameData, "LoadOnDemand");
            System.IO.Directory.CreateDirectory(lodDir); // todo: is this enought to verify the directory exists?
            Current.cfgFileLocation = new System.IO.FileInfo(System.IO.Path.Combine(lodDir, "LoadOnDemand.cfg"));

            if (!Current.cfgFileLocation.Exists)
            {
                ("No config found, starting with defaults.").Log();
            }
            else
            {
                Current.Load(ConfigNode.Load(Current.cfgFileLocation.FullName));
                ("Default config loaded. Default high res settings: " + Current.DefaultImageSettings.HighResCompress).Log();
            }


            ("Todo19: Consider a mechanism to remove old/unsued cached files! (both dead refs and unused cfgs)").Log();
        }
        void Save()
        {
            var cfg = new ConfigNode("LOD_CONFIG");

            var ui = cfg.AddNode("ActivityInterface");
            ui.AddValue("SecondsBeforeShowing", UI_DelayBeforeShowingActivityUI.TotalSeconds.ToInt().ToString());
            ui.AddValue("SecondsBeforeHiding", UI_DelayBeforeHidingActivityUI.TotalSeconds.ToInt().ToString());

            cfg.AddValue("TryUseToolbarForDebugUI", UI_TryUseToolbarForDebugUI.ToString());
            cfg.AddValue("ShowDebugUI", UI_DisplayDebugUI.ToString());
            cfg.AddValue("DontLoadEditorCatalogParts", Debug_DontLoadEditorCatalogThumbnailParts.ToString());

            DefaultImageSettings.WriteToConfigNode(cfg.AddNode("DefaultImageConfig"));

            var cache = cfg.AddNode("Cache");

            foreach (var el in CachedDataPerResUrl)
            {
                cache.AddNode(el.Value.ToConfigNode());
            }

            cfg.Save(cfgFileLocation.FullName);
            IsDirty = false;
        }
        void Load(ConfigNode cfgNode)
        {

            if (cfgNode.HasNode("DefaultImageConfig"))
            {
                DefaultImageSettings = ImageSettings.Combine(ImageSettings.FromConfigNode(cfgNode.GetNode("DefaultImageConfig")), DefaultImageSettings);
            }
            if (cfgNode.HasNode("ActivityInterface"))
            {
                var uiNode = cfgNode.GetNode("ActivityInterface");
                Current.UI_DelayBeforeShowingActivityUI = uiNode.GetValue("SecondsBeforeShowing", s => TimeSpan.FromSeconds(int.Parse(s)), Current.UI_DelayBeforeShowingActivityUI);
                Current.UI_DelayBeforeHidingActivityUI = uiNode.GetValue("SecondsBeforeHiding", s => TimeSpan.FromSeconds(int.Parse(s)), Current.UI_DelayBeforeHidingActivityUI);
            }


            UI_TryUseToolbarForDebugUI = cfgNode.GetValue("TryUseToolbarForDebugUI", text => bool.Parse(text), UI_TryUseToolbarForDebugUI);
            UI_DisplayDebugUI = cfgNode.GetValue("ShowDebugUI", text => bool.Parse(text), UI_DisplayDebugUI);
            Debug_DontLoadEditorCatalogThumbnailParts = cfgNode.GetValue("DontLoadEditorCatalogParts", text => bool.Parse(text), Debug_DontLoadEditorCatalogThumbnailParts);

            if (cfgNode.HasNode("Cache"))
            {
                var cache = cfgNode.GetNode("Cache");
                foreach (ConfigNode node in cache.nodes)
                {
                    var el = ImageConfigItem.FromConfigNode(node);
                    Current.CachedDataPerResUrl[el.FileUrl] = el;
                    if (el.CacheKey != null)
                    {
                        Current.CachedDataPerKey[el.CacheKey] = el;
                    }
                }
            }
        }

        public TimeSpan UI_DelayBeforeShowingActivityUI { get; private set; }
        public TimeSpan UI_DelayBeforeHidingActivityUI { get; private set; }

        public bool UI_TryUseToolbarForDebugUI { get; private set; }
        public bool UI_DisplayDebugUI { get; private set; }
        public bool Debug_DontLoadEditorCatalogThumbnailParts { get; private set; }

        public ImageSettings DefaultImageSettings { get; private set; }

        public bool IsDirty { get; private set; }

        Dictionary<string, ImageConfigItem> CachedDataPerKey = new Dictionary<string, ImageConfigItem>();
        Dictionary<string, ImageConfigItem> CachedDataPerResUrl = new Dictionary<string, ImageConfigItem>();

        /// <summary>
        /// Path of the image cache directory, ready to slap a cachefile-string onto it.
        /// </summary>
        /// <returns></returns>
        public string GetCacheDirectory()
        {
            return cfgFileLocation.Directory.FullName;
        }

        ImageConfigItem addFileToCache(string key, UrlDir.UrlFile file)
        {
            var item = new ImageConfigItem(key, file);
            item.clearCache(GetCacheDirectory());
            CachedDataPerKey.Add(key, item);
            CachedDataPerResUrl.Add(item.FileUrl, item);
            IsDirty = true;
            return item;
        }

        /// <summary>
        /// Trys to find an cache file for an image or creates a new one
        /// </summary>
        /// <param name="file">file to create the cache key for</param>
        /// <returns>name (neither path, nor extension) of a unique image file</returns>
        public ImageConfigItem GetImageConfig(UrlDir.UrlFile file)
        {
            /*
            "CacheKeyTest".Log();
            ("Trg: " + file.fullPath).Log();
            ("Url: " + file.url).Log();
            ("Ext: " + file.fileExtension).Log();
            var p =  file.parent;
            while (p != null)
            {
                ("P: " + (p.name ?? "") + " / " + p.url + "  " + p.path).Log();
                p = p.parent;
            }*/



            ImageConfigItem cachedItem;
            var key = file.url + "." + file.fileExtension;
            if (CachedDataPerResUrl.TryGetValue(key, out cachedItem))
            {
                if (!cachedItem.isCacheValid(file))
                {
                    cachedItem.clearCache(GetCacheDirectory());
                    cachedItem.updateVerificationData(file);
                    IsDirty = true;
                }
                return cachedItem;
            }
            else
            {
                ("New Cache: " + key).Log();
            }
            while (true)
            {
                var cacheKey = RandomString();
                if (!CachedDataPerKey.ContainsKey(cacheKey))
                {
                    return addFileToCache(cacheKey, file);
                }
            }
        }

        static string AllowedChars = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
        static string RandomString(int len = 16)
        {
            var chars = new char[len];
            for (var i = 0; i < len; i++)
            {
                chars[i] = AllowedChars[UnityEngine.Random.Range(0, AllowedChars.Length)];
            }
            return new string(chars);
        }

        /// <summary>
        /// A bunch of changes might have been applied => consider saving.
        /// </summary>
        public void CommitChanges()
        {
            if (IsDirty)
            {
                Save();
            }
        }
    }
}
