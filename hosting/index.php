<?php
/**
 * GAMELAND - Official Gaming Portal & Community Hub
 * Counter-Strike 1.6 Advanced Ecosystem
 * Version: 2.0 Pro
 */
header('Content-Type: text/html; charset=utf-8');
?>
<!DOCTYPE html>
<html lang="fa" dir="rtl">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <meta http-equiv="X-UA-Compatible" content="ie=edge">
  
  <title>GAMELAND | اکوسیستم پیشرفته سرورها و کلاینت کانتر استرایک ۱.۶</title>
  <meta name="description" content="پرتال رسمی گیم‌لند - ارائه‌دهنده سرورهای پرسرعت، کلاینت اختصاصی مجهز به آنتی‌چیت قدرتمند، همگام‌سازی ابری کانفیگ و تجربه بی‌نظیر گیمینگ کانتر استرایک ۱.۶">
  <meta name="keywords" content="کانتر استرایک, گیم لند, دانلود کانتر, سرور کانتر ۱.۶, آنتی چیت, کانفیگ کانتر, Gameland, CS 1.6, NextClient">
  <meta name="theme-color" content="#080a0f">
  
  <!-- Open Graph / Social Media Meta -->
  <meta property="og:title" content="GAMELAND | مرجع تخصصی گیمینگ و سرورهای کانتر ۱.۶">
  <meta property="og:description" content="سریع‌ترین سرورها با پینگ تک‌رقمی، سیستم امنیتی سخت‌افزاری و کلاینت هوشمند گیم‌لند.">
  <meta property="og:type" content="website">
  <meta property="og:image" content="assets/images/logo.svg">
  
  <!-- Favicon -->
  <link rel="icon" type="image/svg+xml" href="assets/images/favicon.svg">
  
  <!-- Google Fonts (Vazirmatn + Outfit for Cyber/Gaming Numbers) -->
  <link rel="preconnect" href="https://fonts.googleapis.com">
  <link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
  <link href="https://fonts.googleapis.com/css2?family=Outfit:wght@400;600;700;800;900&family=Vazirmatn:wght@300;400;500;600;700;800;900&display=swap" rel="stylesheet">
  
  <!-- Main Stylesheet -->
  <link rel="stylesheet" href="assets/css/style.css">
</head>
<body>

  <!-- Cyber Background Grid & Canvas -->
  <div class="cyber-grid-overlay"></div>
  <canvas id="ambient-canvas"></canvas>

  <!-- ========================================================================
       SITE NAVIGATION HEADER
       ======================================================================== -->
  <header class="site-header">
    <div class="container">
      <nav class="nav-wrap">
        <a href="index.php" class="brand-link" aria-label="گیم‌لند خانه">
          <img src="assets/images/logo.svg" alt="GAMELAND Logo" class="brand-logo-img">
          <div class="brand-titles">
            <span class="brand-name">GAMELAND</span>
            <span class="brand-tagline">CS 1.6 ELITE GAMING</span>
          </div>
        </a>

        <ul class="nav-links">
          <li><a href="#hero">صفحه اصلی</a></li>
          <li><a href="#servers">سرورهای فعال</a></li>
          <li><a href="#features">امکانات ویژه</a></li>
          <li><a href="#download">دانلود کلاینت</a></li>
          <li><a href="#faq">سوالات متداول</a></li>
        </ul>

        <div class="nav-cta">
          <a href="#servers" class="btn btn-outline">
            <span>لیست سرورها</span>
          </a>
          <a href="#download" class="btn btn-primary">
            <span>دریافت بازی</span>
            <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round" class="btn-icon-right"><path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"></path><polyline points="7 10 12 15 17 10"></polyline><line x1="12" y1="15" x2="12" y2="3"></line></svg>
          </a>
        </div>

        <button class="mobile-menu-btn" aria-label="منوی موبایل" onclick="document.querySelector('.nav-links').classList.toggle('active')">
          <svg width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><line x1="3" y1="12" x2="21" y2="12"></line><line x1="3" y1="6" x2="21" y2="6"></line><line x1="3" y1="18" x2="21" y2="18"></line></svg>
        </button>
      </nav>
    </div>
  </header>

  <!-- ========================================================================
       HERO SECTION
       ======================================================================== -->
  <section class="hero-section" id="hero">
    <div class="container">
      <div class="hero-grid">
        <div class="hero-content">
          <div class="hero-badge">
            <span class="hero-badge-pulse"></span>
            <span>نسل جدید گیمینگ کانتر استرایک ۱.۶ در ایران</span>
          </div>
          
          <h1 class="hero-title">
            تجربه‌ای روان، امن و مدرن با پورتال <span class="highlight">GAMELAND</span>
          </h1>

          <p class="hero-description">
            به خانواده بزرگ گیم‌لند خوش آمدید. مجهز به سرورهای ۱۰۰۰ تیک ریت با پینگ فوق‌العاده پایین، کلاینت اختصاصی به همراه سیستم امنیتی سخت‌افزاری ضد چیت، قابلیت همگام‌سازی ابری کانفیگ بازیکنان و سیستم ضبط خودکار دمو.
          </p>

          <div class="hero-actions">
            <a href="#download" class="btn btn-primary btn-lg">
              <svg width="22" height="22" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round"><path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"></path><polyline points="7 10 12 15 17 10"></polyline><line x1="12" y1="15" x2="12" y2="3"></line></svg>
              <span>دانلود نسخه جدید کلاینت</span>
            </a>
            <a href="#servers" class="btn btn-outline btn-lg">
              <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><polygon points="5 3 19 12 5 21 5 3"></polygon></svg>
              <span>ورود مستقیم به سرورها</span>
            </a>
          </div>

          <div class="hero-stats-bar">
            <div class="hero-stat-item">
              <div class="stat-number">1000<span class="unit">FPS</span></div>
              <div class="stat-label">تیک‌ریت فوق‌روان سرورها</div>
            </div>
            <div class="hero-stat-item">
              <div class="stat-number">99.9<span class="unit">%</span></div>
              <div class="stat-label">آپ‌تایم و پایداری شبکه</div>
            </div>
            <div class="hero-stat-item">
              <div class="stat-number">&lt; 15<span class="unit">ms</span></div>
              <div class="stat-label">میانگین پینگ سرورهای داخلی</div>
            </div>
          </div>
        </div>

        <div class="hero-visual">
          <div class="hero-card-glow"></div>
          <div class="tactical-display-box">
            <div class="display-header">
              <div class="display-tag">
                <span class="pulse-dot"></span>
                <span>GAMELAND SECURE ENGINE</span>
              </div>
              <span class="display-badge">STABLE v2.4</span>
            </div>

            <div class="showcase-features-list">
              <div class="showcase-feature-row">
                <div class="feat-info">
                  <div class="feat-icon">🛡️</div>
                  <div>
                    <div class="feat-title">سیستم امنیتی گیم‌لند</div>
                    <div class="feat-sub">مسدودسازی خودکار چیت و کدهای مخرب</div>
                  </div>
                </div>
                <span class="feat-status status-active">فعال و ایمن</span>
              </div>

              <div class="showcase-feature-row">
                <div class="feat-info">
                  <div class="feat-icon">⚡</div>
                  <div>
                    <div class="feat-title">پینگ و ریکویل بهینه‌سازی‌شده</div>
                    <div class="feat-sub">مسیریابی هوشمند سرورهای ایران</div>
                  </div>
                </div>
                <span class="feat-status status-ultra">ULTRA LOW</span>
              </div>

              <div class="showcase-feature-row">
                <div class="feat-info">
                  <div class="feat-icon">☁️</div>
                  <div>
                    <div class="feat-title">همگام‌سازی ابری تنظیمات (Cloud Sync)</div>
                    <div class="feat-sub">ذخیره خودکار Config و کلیدهای شخصی</div>
                  </div>
                </div>
                <span class="feat-status status-active">SYNCHRONIZED</span>
              </div>

              <div class="showcase-feature-row">
                <div class="feat-info">
                  <div class="feat-icon">🎬</div>
                  <div>
                    <div class="feat-title">آرشیو خودکار دمو (Auto Demo)</div>
                    <div class="feat-sub">آپلود و نگهداری دموهای مسابقات و مچ‌ها</div>
                  </div>
                </div>
                <span class="feat-status status-secure">ذخیره ابری</span>
              </div>
            </div>
          </div>
        </div>
      </div>
    </div>
  </section>

  <!-- ========================================================================
       LIVE SERVERS SECTION
       ======================================================================== -->
  <section id="servers">
    <div class="container">
      <div class="section-head">
        <span class="section-tag">LIVE BATTLEGROUNDS</span>
        <h2 class="section-title">سرورهای آنلاین و اختصاصی گیم‌لند</h2>
        <p class="section-desc">برای اتصال به سرورها روی دکمه کپی آی‌پی کلیک کرده و در کنسول بازی (کلید ~) دستور connect را Paste کنید.</p>
      </div>

      <div class="servers-grid">
        <!-- Server 1: Public Pro -->
        <div class="server-card">
          <div class="server-banner" style="background: linear-gradient(180deg, rgba(8,10,15,0.2), rgba(13,17,23,0.9)), radial-gradient(circle at center, #1b2838 0%, #0a0b0e 100%);">
            <span class="server-mode-badge">PUBLIC MATCH</span>
          </div>
          <div class="server-body">
            <div class="server-header-row">
              <h3 class="server-name">GAMELAND | Public Pro #1</h3>
              <div class="server-ping">
                <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M2 20h.01M7 20v-4M12 20v-8M17 20V4"/></svg>
                <span data-base-ping="12">12ms</span>
              </div>
            </div>
            <div class="server-meta-row">
              <div class="meta-map">
                <span>نقشه:</span>
                <strong style="color: #fff;">de_dust2</strong>
              </div>
              <div class="meta-players">
                <span>بازیکنان:</span>
                <strong style="color: var(--accent-neon-bright);">28 / 32</strong>
              </div>
            </div>
            <div class="server-progress">
              <div class="progress-fill" style="width: 87%;"></div>
            </div>
            <div class="server-action-box">
              <span class="server-ip-text">cs.gameland.cam:27015</span>
              <button class="btn-copy-ip" data-ip="cs.gameland.cam:27015">کپی آی‌پی</button>
            </div>
          </div>
        </div>

        <!-- Server 2: Match & Competitive -->
        <div class="server-card">
          <div class="server-banner" style="background: linear-gradient(180deg, rgba(8,10,15,0.2), rgba(13,17,23,0.9)), radial-gradient(circle at center, #2b1f3d 0%, #0a0b0e 100%);">
            <span class="server-mode-badge" style="border-color: rgba(0,180,216,0.4); background: rgba(0,180,216,0.2); color: #00b4d8;">COMPETITIVE 5V5</span>
          </div>
          <div class="server-body">
            <div class="server-header-row">
              <h3 class="server-name">GAMELAND | Match / ClanWar</h3>
              <div class="server-ping">
                <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M2 20h.01M7 20v-4M12 20v-8M17 20V4"/></svg>
                <span data-base-ping="14">14ms</span>
              </div>
            </div>
            <div class="server-meta-row">
              <div class="meta-map">
                <span>نقشه:</span>
                <strong style="color: #fff;">de_inferno</strong>
              </div>
              <div class="meta-players">
                <span>بازیکنان:</span>
                <strong style="color: var(--accent-cyan);">10 / 12</strong>
              </div>
            </div>
            <div class="server-progress">
              <div class="progress-fill" style="width: 83%; background: linear-gradient(90deg, #00b4d8, #00e599);"></div>
            </div>
            <div class="server-action-box">
              <span class="server-ip-text">cs.gameland.cam:27016</span>
              <button class="btn-copy-ip" data-ip="cs.gameland.cam:27016">کپی آی‌پی</button>
            </div>
          </div>
        </div>

        <!-- Server 3: CSDM / Deathmatch -->
        <div class="server-card">
          <div class="server-banner" style="background: linear-gradient(180deg, rgba(8,10,15,0.2), rgba(13,17,23,0.9)), radial-gradient(circle at center, #38241b 0%, #0a0b0e 100%);">
            <span class="server-mode-badge" style="border-color: rgba(255,183,3,0.4); background: rgba(255,183,3,0.2); color: #ffb703;">CSDM RESPAWN</span>
          </div>
          <div class="server-body">
            <div class="server-header-row">
              <h3 class="server-name">GAMELAND | Deathmatch FFA</h3>
              <div class="server-ping">
                <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M2 20h.01M7 20v-4M12 20v-8M17 20V4"/></svg>
                <span data-base-ping="11">11ms</span>
              </div>
            </div>
            <div class="server-meta-row">
              <div class="meta-map">
                <span>نقشه:</span>
                <strong style="color: #fff;">de_mirage</strong>
              </div>
              <div class="meta-players">
                <span>بازیکنان:</span>
                <strong style="color: var(--accent-amber);">24 / 32</strong>
              </div>
            </div>
            <div class="server-progress">
              <div class="progress-fill" style="width: 75%; background: linear-gradient(90deg, #ffb703, #ff0055);"></div>
            </div>
            <div class="server-action-box">
              <span class="server-ip-text">cs.gameland.cam:27017</span>
              <button class="btn-copy-ip" data-ip="cs.gameland.cam:27017">کپی آی‌پی</button>
            </div>
          </div>
        </div>
      </div>
    </div>
  </section>

  <!-- ========================================================================
       FEATURES SECTION
       ======================================================================== -->
  <section id="features" style="background: rgba(13, 17, 23, 0.4);">
    <div class="container">
      <div class="section-head">
        <span class="section-tag">ELITE ECOSYSTEM</span>
        <h2 class="section-title">چرا پلتفرم گیم‌لند متمایز است؟</h2>
        <p class="section-desc">زیرساختی که برای حرفه‌ای‌ترین پلیرهای شوتر کلاسیک ایران با دقت مهندسی شده است.</p>
      </div>

      <div class="features-grid">
        <div class="feature-card">
          <div class="feature-icon-wrapper">🛡️</div>
          <h3 class="feature-card-title">آنتی‌چیت سخت‌افزاری</h3>
          <p class="feature-card-desc">محافظت تمام‌عیار از سلامت رقابت‌ها. مسدودسازی الیاس‌های نامتعارف، چیت‌های وال‌هک، ایم‌بات و تغییرات غیرمجاز در کلاینت بازی.</p>
        </div>

        <div class="feature-card">
          <div class="feature-icon-wrapper">☁️</div>
          <h3 class="feature-card-title">همگام‌سازی ابری کانفیگ</h3>
          <p class="feature-card-desc">دیگر نگران حذف یا تغییر کدهای شخصی خود نباشید! تمام تنظیمات، نشانه‌گیر و کلیدهای شما در سرور ابری گیم‌لند ذخیره می‌گردد.</p>
        </div>

        <div class="feature-card">
          <div class="feature-icon-wrapper">📹</div>
          <h3 class="feature-card-title">سیستم مدیریت خودکار دمو</h3>
          <p class="feature-card-desc">تمام مسابقات شما به شکل اتوماتیک ضبط و با فشرده‌سازی در هاست ذخیره می‌شوند تا به سادگی به بازبینی صحنه‌ها دسترسی داشته باشید.</p>
        </div>

        <div class="feature-card">
          <div class="feature-icon-wrapper">🚀</div>
          <h3 class="feature-card-title">موتور بهینه‌ساز ۱۰۰۰ FPS</h3>
          <p class="feature-card-desc">تیونینگ حرفه‌ای سرورها و پچ گرافیکی کلاینت جهت ثبت نرم‌ترین تیراندازی و کمترین تاخیر در واکنش‌ها بدون افت فریم.</p>
        </div>

        <div class="feature-card">
          <div class="feature-icon-wrapper">🔐</div>
          <h3 class="feature-card-title">ثبت‌نام پیامکی امن (OTP)</h3>
          <p class="feature-card-desc">سیستم احراز هویت هوشمند همراه با بازیابی کلمه عبور پیامکی بدون نیاز به ایمیل یا روندهای پیچیده، با بالاترین استاندارد امنیت.</p>
        </div>

        <div class="feature-card">
          <div class="feature-icon-wrapper">🏆</div>
          <h3 class="feature-card-title">رنکینگ و مسابقات دوره‌ای</h3>
          <p class="feature-card-desc">ثبت امتیازات کیل، هدشات و MVP در پایگاه‌داده به همراه جوایز نقدی و اشتراک‌های ویژه برای برترین پلیرهای ماهانه.</p>
        </div>
      </div>
    </div>
  </section>

  <!-- ========================================================================
       DOWNLOAD HUB SECTION
       ======================================================================== -->
  <section id="download">
    <div class="container">
      <div class="section-head">
        <span class="section-tag">OFFICIAL CLIENT</span>
        <h2 class="section-title">دانلود و راه‌اندازی کلاینت گیم‌لند</h2>
        <p class="section-desc">پکیج رسمی را با بالاترین سرعت از سرورهای اختصاصی گیم‌لند دریافت نمایید.</p>
      </div>

      <div class="download-wrapper">
        <div class="download-tabs">
          <button class="download-tab-btn active" data-tab="tab-full">پکیج کامل بازی (Full Setup)</button>
          <button class="download-tab-btn" data-tab="tab-patch">پچ هوشمند (Smart Patch)</button>
        </div>

        <!-- Tab 1: Full Installer -->
        <div class="download-content-pane active" id="tab-full">
          <div class="download-detail-grid">
            <div class="download-info">
              <h3>کلاینت کامل و ارجینال GAMELAND CS 1.6</h3>
              <p>این نسخه حاوی تمامی فایل‌های پایه بازی به همراه جدیدترین پچ‌های گرافیکی، سیستم صوتی سه‌بعدی، مدل‌های استاندارد مسابقاتی، و کلاینت فوق‌سریع نسل جدید است. کاملاً آماده و بدون نیاز به نصب هیچ‌گونه فایل اضافی.</p>

              <div class="download-specs">
                <div class="spec-box">
                  <div class="spec-title">حجم فایل نصبی</div>
                  <div class="spec-val">~ 280 MB</div>
                </div>
                <div class="spec-box">
                  <div class="spec-title">سیستم عامل</div>
                  <div class="spec-val">Windows 7 / 8 / 10 / 11</div>
                </div>
                <div class="spec-box">
                  <div class="spec-title">نسخه بیلد</div>
                  <div class="spec-val">v2.4.0 Final</div>
                </div>
                <div class="spec-box">
                  <div class="spec-title">نوع نصب</div>
                  <div class="spec-val">نصب اتوماتیک با اینستالر هوشمند</div>
                </div>
              </div>

              <a href="download/gameland_setup.exe" class="btn btn-primary btn-lg" style="width: 100%;">
                <svg width="22" height="22" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5"><path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"></path><polyline points="7 10 12 15 17 10"></polyline><line x1="12" y1="15" x2="12" y2="3"></line></svg>
                <span>دانلود مستقیم نسخه کامل (لینک مستقیم سرور ایران)</span>
              </a>
            </div>

            <div class="download-box-cta">
              <div class="download-box-icon">⚡</div>
              <h4 style="font-size: 1.25rem; font-weight: 800; color: #fff; margin-bottom: 8px;">سرور پرسرعت داخلی</h4>
              <p style="font-size: 0.9rem; color: var(--text-muted); line-height: 1.6;">ترافیک دانلود به صورت نیم‌بها محاسبه شده و با حداکثر سرعت خط شما دریافت می‌گردد.</p>
              <span class="download-box-note">تست سلامت فایل‌ها: SHA256 Verified ✓</span>
            </div>
          </div>
        </div>

        <!-- Tab 2: Smart Patch -->
        <div class="download-content-pane" id="tab-patch">
          <div class="download-detail-grid">
            <div class="download-info">
              <h3>پچ فوق‌سبک و هوشمند گیم‌لند</h3>
              <p>اگر از قبل کانتر استرایک ۱.۶ را بر روی سیستم خود نصب دارید، نیازی به دانلود نسخه کامل نیست. پچ هوشمند گیم‌لند به صورت خودکار پوشه بازی شما را یافته و تنها ماژول‌های امنیتی، لانچر و اتصال به سرور را بروزرسانی می‌کند (بدون دستکاری مپ‌ها و کانفیگ‌های قبلی شما).</p>

              <div class="download-specs">
                <div class="spec-box">
                  <div class="spec-title">حجم پچ هوشمند</div>
                  <div class="spec-val">~ 12 MB</div>
                </div>
                <div class="spec-box">
                  <div class="spec-title">سازگاری</div>
                  <div class="spec-val">تمامی نسخه‌های CS 1.6</div>
                </div>
                <div class="spec-box">
                  <div class="spec-title">حفظ نقشه‌ها</div>
                  <div class="spec-val">100% تضمینی</div>
                </div>
                <div class="spec-box">
                  <div class="spec-title">زمان نصب</div>
                  <div class="spec-val">کمتر از ۵ ثانیه</div>
                </div>
              </div>

              <a href="download/gameland_patch.exe" class="btn btn-primary btn-lg" style="width: 100%;">
                <svg width="22" height="22" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5"><path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"></path><polyline points="7 10 12 15 17 10"></polyline><line x1="12" y1="15" x2="12" y2="3"></line></svg>
                <span>دانلود پچ هوشمند و سریع گیم‌لند</span>
              </a>
            </div>

            <div class="download-box-cta">
              <div class="download-box-icon" style="background: rgba(0, 180, 216, 0.15); color: var(--accent-cyan);">🛠️</div>
              <h4 style="font-size: 1.25rem; font-weight: 800; color: #fff; margin-bottom: 8px;">نصب بدون تغییر مپ‌ها</h4>
              <p style="font-size: 0.9rem; color: var(--text-muted); line-height: 1.6;">اینستالر دو تکه هوشمند به هیچ وجه مپ‌های دانلودی شما را حذف نخواهد کرد.</p>
              <span class="download-box-note">پشتیبانی کامل از تمامی استیم و نان‌استیم‌ها</span>
            </div>
          </div>
        </div>
      </div>
    </div>
  </section>

  <!-- ========================================================================
       FAQ SECTION
       ======================================================================== -->
  <section id="faq">
    <div class="container">
      <div class="section-head">
        <span class="section-tag">HELP & SUPPORT</span>
        <h2 class="section-title">پرسش‌های پرتکرار</h2>
        <p class="section-desc">پاسخ سریع به متداول‌ترین سوالات پلیرهای عزیز گیم‌لند.</p>
      </div>

      <div class="faq-accordion">
        <div class="faq-item">
          <button class="faq-question">
            <span>چگونه می‌توانم در سرورهای گیم‌لند بازی کنم؟</span>
            <span class="faq-toggle-icon">▼</span>
          </button>
          <div class="faq-answer">
            کافیست کلاینت گیم‌لند را از بخش دانلود دریافت کرده و نصب نمایید. همچنین می‌توانید با کلاینت فعلی خود، آی‌پی سرورها را کپی کرده و در کنسول بازی (کلید ~) با دستور connect [IP] وارد سرور شوید.
          </div>
        </div>

        <div class="faq-item">
          <button class="faq-question">
            <span>آیا ثبت‌نام با شماره موبایل الزامی است؟</span>
            <span class="faq-toggle-icon">▼</span>
          </button>
          <div class="faq-answer">
            خیر، برای ورود به سرورهای عمومی الزامی نیست، اما ثبت‌نام در سامانه ابری گیم‌لند امکان ذخیره آنلاین کانفیگ، بازیابی آسان کلمه عبور و شرکت در رنکینگ ماهانه را برای شما فعال می‌سازد. هر شماره در ماه ۵ سهمیه ارسال پیامک رایگان دارد.
          </div>
        </div>

        <div class="faq-item">
          <button class="faq-question">
            <span>سیستم همگام‌سازی ابری کانفیگ (Cloud Sync) چگونه کار می‌کند؟</span>
            <span class="faq-toggle-icon">▼</span>
          </button>
          <div class="faq-answer">
            با ورود به حساب کاربری خود در لانچر بازی، تنظیمات و کلیدهای شخصی شما به شکل خودکار بر روی سرور ابری گیم‌لند ذخیره می‌شوند. در صورتی که در سیستم دیگری لاگین کنید، تمام تنظیمات شما در لحظه بارگذاری خواهد شد.
          </div>
        </div>

        <div class="faq-item">
          <button class="faq-question">
            <span>آیا فایل‌های مپ من هنگام آپدیت حذف می‌شوند؟</span>
            <span class="faq-toggle-icon">▼</span>
          </button>
          <div class="faq-answer">
            خیر! اینستالر هوشمند گیم‌لند با الگوریتم شناسایی امن طراحی شده و پوشه‌های maps و sounds بازی شما را دست‌نخورده حفظ می‌نماید.
          </div>
        </div>
      </div>
    </div>
  </section>

  <!-- ========================================================================
       FOOTER
       ======================================================================== -->
  <footer class="site-footer">
    <div class="container">
      <div class="footer-grid">
        <div>
          <a href="index.php" class="brand-link">
            <img src="assets/images/logo.svg" alt="GAMELAND Logo" style="width: 38px; height: 38px;">
            <span class="brand-name">GAMELAND</span>
          </a>
          <p class="footer-brand-desc">
            پروژه گیم‌لند، برترین اکوسیستم گیمینگ کانتر استرایک ۱.۶ با هدف ارائه تجربه‌ای پاک، بدون چیت، پایدار و سرشار از هیجان برای جامعه گیمرهای ایرانی.
          </p>
        </div>

        <div>
          <h4 class="footer-col-title">دسترسی سریع</h4>
          <ul class="footer-links">
            <li><a href="#hero">صفحه نخست</a></li>
            <li><a href="#servers">سرورهای آنلاین</a></li>
            <li><a href="#features">امکانات ویژه</a></li>
            <li><a href="#download">دانلود کلاینت رسمی</a></li>
          </ul>
        </div>

        <div>
          <h4 class="footer-col-title">ارتباط با جامعه گیم‌لند</h4>
          <ul class="footer-links">
            <li><a href="https://t.me/gameland" target="_blank" rel="noopener">کانال اطلاع‌رسانی تلگرام</a></li>
            <li><a href="https://discord.gg/gameland" target="_blank" rel="noopener">سرور گفتگوی دیسکورد</a></li>
            <li><a href="mailto:support@gameland.cam">پشتیبانی ایمیل</a></li>
            <li><a href="admin/">پنل مدیریت هاست</a></li>
          </ul>
        </div>
      </div>

      <div class="footer-bottom">
        <div>© ۲۰۲۶ GAMELAND - تمامی حقوق محفوظ است. طراحی و پیاده‌سازی شده با بالاترین استانداردهای گیمینگ.</div>
        <div style="font-family: var(--font-gaming); color: var(--accent-emerald);">SECURED BY GAMELAND ENGINE v2.4</div>
      </div>
    </div>
  </footer>

  <!-- Toast Notification element for 1-click IP copy -->
  <div class="toast-container">
    <div id="toast" class="toast">
      <span class="toast-icon">✓</span>
      <span id="toast-msg">آدرس سرور با موفقیت کپی شد!</span>
    </div>
  </div>

  <!-- Main JavaScript File -->
  <script src="assets/js/main.js"></script>
</body>
</html>
