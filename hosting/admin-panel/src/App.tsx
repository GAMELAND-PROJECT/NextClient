import { useState, useEffect } from 'react'
import { Upload, Monitor, History, LogOut, FileArchive, CheckCircle } from 'lucide-react'

// Backend URL
const API_URL = '/api.php'

function App() {
  const [activeTab, setActiveTab] = useState('gamenets')
  const [tags, setTags] = useState([])
  const [demos, setDemos] = useState([])
  
  const [uploadTag, setUploadTag] = useState('')
  const [uploadVersion, setUploadVersion] = useState('')
  const [uploadFile, setUploadFile] = useState<File | null>(null)
  const [uploading, setUploading] = useState(false)
  const [uploadSuccess, setUploadSuccess] = useState(false)

  useEffect(() => {
    if (activeTab === 'gamenets') fetchTags()
    if (activeTab === 'demos') fetchDemos()
  }, [activeTab])

  const fetchTags = async () => {
    try {
      const res = await fetch(`${API_URL}?action=get_tags`)
      const data = await res.json()
      if (data.success) setTags(data.tags)
    } catch (e) { console.error(e) }
  }

  const fetchDemos = async () => {
    try {
      const res = await fetch(`${API_URL}?action=get_demos`)
      const data = await res.json()
      if (data.success) setDemos(data.demos)
    } catch (e) { console.error(e) }
  }

  const handleUpload = async (e: React.FormEvent) => {
    e.preventDefault()
    if (!uploadFile || !uploadTag || !uploadVersion) return
    
    setUploading(true)
    const formData = new FormData()
    formData.append('tag', uploadTag)
    formData.append('version', uploadVersion)
    formData.append('file', uploadFile)

    try {
      const res = await fetch(`${API_URL}?action=upload_update`, {
        method: 'POST',
        body: formData
      })
      const data = await res.json()
      if (data.success) {
        setUploadSuccess(true)
        setTimeout(() => {
          setUploadSuccess(false)
          setActiveTab('gamenets')
        }, 2000)
      } else {
        alert("خطا در آپلود: " + data.error)
      }
    } catch (e) {
      alert("خطا در ارتباط با سرور")
    }
    setUploading(false)
  }

  return (
    <div className="min-h-screen flex bg-[#0f172a] text-slate-200" dir="rtl">
      {/* Sidebar */}
      <div className="w-64 glass border-r border-slate-700/50 p-6 flex flex-col gap-6">
        <div className="flex items-center gap-3 mb-4">
          <div className="w-10 h-10 rounded-xl bg-gradient-to-br from-indigo-500 to-purple-600 flex items-center justify-center shadow-lg shadow-indigo-500/20">
            <Monitor className="text-white" size={20} />
          </div>
          <h1 className="font-bold text-xl tracking-wide bg-clip-text text-transparent bg-gradient-to-r from-white to-slate-400">NextClient</h1>
        </div>

        <nav className="flex-1 space-y-2">
          <button 
            onClick={() => setActiveTab('gamenets')}
            className={`w-full flex items-center gap-3 px-4 py-3 rounded-lg transition-all ${activeTab === 'gamenets' ? 'bg-indigo-500/10 text-indigo-400 font-medium' : 'hover:bg-slate-800/50 text-slate-400 hover:text-slate-200'}`}
          >
            <Monitor size={18} />
            مدیریت گیم‌نت‌ها
          </button>
          
          <button 
            onClick={() => {
              setActiveTab('upload')
              setUploadTag('')
              setUploadVersion('')
              setUploadFile(null)
            }}
            className={`w-full flex items-center gap-3 px-4 py-3 rounded-lg transition-all ${activeTab === 'upload' ? 'bg-indigo-500/10 text-indigo-400 font-medium' : 'hover:bg-slate-800/50 text-slate-400 hover:text-slate-200'}`}
          >
            <Upload size={18} />
            آپلود آپدیت
          </button>

          <button 
            onClick={() => setActiveTab('demos')}
            className={`w-full flex items-center gap-3 px-4 py-3 rounded-lg transition-all ${activeTab === 'demos' ? 'bg-indigo-500/10 text-indigo-400 font-medium' : 'hover:bg-slate-800/50 text-slate-400 hover:text-slate-200'}`}
          >
            <History size={18} />
            آرشیو دموها
          </button>
        </nav>

        <button className="w-full flex items-center gap-3 px-4 py-3 rounded-lg text-rose-400 hover:bg-rose-500/10 transition-colors mt-auto">
          <LogOut size={18} />
          خروج
        </button>
      </div>

      {/* Main Content */}
      <div className="flex-1 p-8 overflow-auto relative">
        {/* Background blobs */}
        <div className="absolute top-[-10%] left-[-10%] w-96 h-96 bg-indigo-500/10 rounded-full blur-3xl pointer-events-none"></div>
        <div className="absolute bottom-[-10%] right-[-10%] w-96 h-96 bg-purple-500/10 rounded-full blur-3xl pointer-events-none"></div>

        <div className="max-w-5xl mx-auto relative z-10">
          
          {activeTab === 'gamenets' && (
            <div className="space-y-6 animate-in fade-in slide-in-from-bottom-4 duration-500">
              <div className="flex items-center justify-between">
                <h2 className="text-2xl font-bold">لیست گیم‌نت‌ها</h2>
                <button 
                  onClick={() => setActiveTab('upload')}
                  className="bg-indigo-500 hover:bg-indigo-600 text-white px-4 py-2 rounded-lg font-medium transition-colors shadow-lg shadow-indigo-500/20 flex items-center gap-2"
                >
                  <Upload size={16} /> آپدیت جدید
                </button>
              </div>

              <div className="glass rounded-xl overflow-hidden">
                <table className="w-full text-right">
                  <thead>
                    <tr className="border-b border-slate-700/50 bg-slate-800/30">
                      <th className="px-6 py-4 font-medium text-slate-400">تگ گیم‌نت</th>
                      <th className="px-6 py-4 font-medium text-slate-400">آخرین نسخه</th>
                      <th className="px-6 py-4 font-medium text-slate-400">تاریخ آخرین آپدیت</th>
                      <th className="px-6 py-4 font-medium text-slate-400">عملیات</th>
                    </tr>
                  </thead>
                  <tbody className="divide-y divide-slate-700/50">
                    {tags.length === 0 ? (
                      <tr><td colSpan={4} className="px-6 py-8 text-center text-slate-500">هیچ گیم‌نتی یافت نشد</td></tr>
                    ) : (
                      tags.map((t: any) => (
                        <tr key={t.tag} className="hover:bg-slate-800/30 transition-colors">
                          <td className="px-6 py-4">
                            <div className="inline-flex items-center px-2.5 py-1 rounded-md bg-slate-700/50 border border-slate-600 text-sm font-medium">
                              {t.tag}
                            </div>
                          </td>
                          <td className="px-6 py-4 text-indigo-400">{t.latest_version}</td>
                          <td className="px-6 py-4 text-slate-400">{t.updated_at}</td>
                          <td className="px-6 py-4">
                            <button 
                              onClick={() => {
                                setUploadTag(t.tag)
                                setActiveTab('upload')
                              }}
                              className="text-sm text-indigo-400 hover:text-indigo-300 transition-colors"
                            >
                              ارسال آپدیت
                            </button>
                          </td>
                        </tr>
                      ))
                    )}
                  </tbody>
                </table>
              </div>
            </div>
          )}

          {activeTab === 'upload' && (
            <div className="max-w-2xl mx-auto space-y-6 animate-in fade-in zoom-in-95 duration-500">
              <h2 className="text-2xl font-bold">بارگذاری آپدیت جدید</h2>
              <form onSubmit={handleUpload} className="glass p-8 rounded-2xl space-y-6">
                
                {uploadSuccess ? (
                  <div className="flex flex-col items-center justify-center py-12 text-emerald-400 space-y-4">
                    <div className="w-16 h-16 bg-emerald-500/10 rounded-full flex items-center justify-center">
                      <CheckCircle size={32} />
                    </div>
                    <h3 className="text-xl font-bold text-white">آپدیت با موفقیت ارسال شد</h3>
                    <p className="text-slate-400">فایل روی سرور دانلود قرار گرفت و دیتابیس بروز شد.</p>
                  </div>
                ) : (
                  <>
                    <div className="space-y-2">
                      <label className="text-sm font-medium text-slate-400">تگ گیم‌نت هدف</label>
                      <select 
                        value={uploadTag} 
                        onChange={e => setUploadTag(e.target.value)}
                        className="w-full bg-slate-900 border border-slate-700 rounded-lg px-4 py-3 outline-none focus:border-indigo-500 transition-colors"
                        required
                      >
                        <option value="">انتخاب کنید...</option>
                        {tags.map((t: any) => <option key={t.tag} value={t.tag}>{t.tag}</option>)}
                      </select>
                    </div>

                    <div className="space-y-2">
                      <label className="text-sm font-medium text-slate-400">شماره نسخه جدید (مثال: 2.6.0)</label>
                      <input 
                        type="text" 
                        value={uploadVersion} 
                        onChange={e => setUploadVersion(e.target.value)}
                        placeholder="v2.x.x"
                        className="w-full bg-slate-900 border border-slate-700 rounded-lg px-4 py-3 outline-none focus:border-indigo-500 transition-colors"
                        required
                        dir="ltr"
                      />
                    </div>

                    <div className="space-y-2">
                      <label className="text-sm font-medium text-slate-400">فایل نصبی آپدیت (.zip یا .exe)</label>
                      <label className="flex flex-col items-center justify-center w-full h-32 border-2 border-dashed border-slate-700 rounded-lg hover:border-indigo-500 hover:bg-slate-800/30 transition-all cursor-pointer">
                        <div className="flex flex-col items-center justify-center pt-5 pb-6">
                          <Upload className="w-8 h-8 text-slate-400 mb-2" />
                          <p className="text-sm text-slate-400">
                            {uploadFile ? uploadFile.name : 'برای انتخاب فایل کلیک کنید یا فایل را اینجا رها کنید'}
                          </p>
                        </div>
                        <input 
                          type="file" 
                          className="hidden" 
                          onChange={e => setUploadFile(e.target.files?.[0] || null)}
                          required
                        />
                      </label>
                    </div>

                    <div className="pt-4">
                      <button 
                        type="submit" 
                        disabled={uploading}
                        className="w-full bg-gradient-to-r from-indigo-500 to-purple-600 hover:from-indigo-600 hover:to-purple-700 text-white font-bold py-3 px-4 rounded-lg shadow-lg shadow-indigo-500/25 transition-all disabled:opacity-50 disabled:cursor-not-allowed"
                      >
                        {uploading ? 'در حال بارگذاری و بروزرسانی...' : 'آپلود و انتشار آپدیت'}
                      </button>
                    </div>
                  </>
                )}
              </form>
            </div>
          )}

          {activeTab === 'demos' && (
            <div className="space-y-6 animate-in fade-in slide-in-from-bottom-4 duration-500">
              <h2 className="text-2xl font-bold">فایل‌های دموی ضبط شده</h2>

              <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-6">
                {demos.length === 0 ? (
                  <div className="col-span-full glass p-12 rounded-xl text-center text-slate-500">
                    <FileArchive className="w-12 h-12 mx-auto mb-4 opacity-20" />
                    <p>هیچ دمویی در سرور یافت نشد.</p>
                  </div>
                ) : (
                  demos.map((d: any) => (
                    <div key={d.filename} className="glass p-5 rounded-xl space-y-4 hover:-translate-y-1 transition-transform cursor-pointer border border-slate-700/50 hover:border-indigo-500/50">
                      <div className="flex items-start justify-between">
                        <div className="p-3 bg-indigo-500/10 text-indigo-400 rounded-lg">
                          <FileArchive size={24} />
                        </div>
                        <span className="text-xs text-slate-500 font-mono">{(d.size / 1024 / 1024).toFixed(2)} MB</span>
                      </div>
                      <div>
                        <h4 className="font-medium text-slate-200 truncate" dir="ltr">{d.filename}</h4>
                        <p className="text-xs text-slate-400 mt-1">{d.date}</p>
                      </div>
                      <a 
                        href={d.url} 
                        download
                        className="block text-center w-full bg-slate-800 hover:bg-slate-700 text-sm font-medium py-2 rounded-md transition-colors"
                      >
                        دانلود دمو
                      </a>
                    </div>
                  ))
                )}
              </div>
            </div>
          )}

        </div>
      </div>
    </div>
  )
}

export default App
