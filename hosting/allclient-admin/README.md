# Allclient control panel

پنل سبک PHP برای مدیریت مستقیم فایل‌های زیر روی `gameland.cam`:

- `pinned_servers.txt`
- `client_tags.txt`
- `server_password.txt`

## نیازمندی

- PHP 8.1 یا جدیدتر
- HTTPS فعال
- Apache/cPanel با امکان اجرای PHP

## نصب در cPanel

1. در `File Manager` وارد `public_html` شوید.
2. پوشه‌ای با نام `admin` بسازید.
3. تمام فایل‌های این پوشه را داخل `public_html/admin` آپلود کنید.
4. فایل `config.example.php` را Copy و با نام `config.php` ذخیره کنید.
5. از بخش `Terminal` در cPanel دستور زیر را با رمز قوی خود اجرا کنید:

   ```bash
   cd ~/public_html/admin
   php make-password.php 'A-VERY-STRONG-PASSWORD'
   ```

6. مقدار خروجی که با `$2y$` شروع می‌شود را در `config.php` جایگزین
   `REPLACE_WITH_PASSWORD_HASH` کنید. خود رمز را داخل فایل قرار ندهید.
7. مطمئن شوید مقدار `data_dir` همین باشد:

   ```php
   'data_dir' => dirname(__DIR__),
   ```

8. از cPanel بخش `Directory Privacy` برای پوشه `public_html/admin` یک لایه رمز
   دوم نیز فعال کنید.
9. پنل را باز کنید:

   ```text
   https://gameland.cam/admin/
   ```

## رفتار ذخیره‌سازی

- ورودی‌ها قبل از ذخیره با قواعد خود کلاینت اعتبارسنجی می‌شوند.
- فایل ابتدا به‌صورت موقت نوشته و سپس اتمیک جایگزین می‌شود.
- قبل از هر تغییر Backup خصوصی در `admin/backups` ساخته می‌شود.
- حداکثر ۳۰ Backup اخیر هر فایل نگهداری می‌شود.
- رمز فعلی سرورها هیچ‌وقت در رابط وب نمایش داده نمی‌شود.

## نکات امنیتی

- پوشه `admin` و فایل `config.php` را داخل GitHub یا ZIP عمومی قرار ندهید.
- رمز cPanel را به‌عنوان رمز پنل استفاده نکنید.
- سطح دسترسی پیشنهادی `config.php` برابر `0600` است.
- اگر هاست Apache 2.2 قدیمی دارد، حتماً Directory Privacy را فعال کنید؛ قواعد
  `.htaccess` این پروژه برای Apache 2.4 نوشته شده‌اند.
