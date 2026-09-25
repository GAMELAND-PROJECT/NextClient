/* ==========================================================================
   GAMELAND CS 1.6 PORTAL - JAVASCRIPT CONTROLLER
   Interactive particle glow, copy to clipboard, tab switching, ping simulator
   ========================================================================== */

document.addEventListener('DOMContentLoaded', () => {
  initNavbarScroll();
  initAmbientParticles();
  initDownloadTabs();
  initFaqAccordion();
  initCopyServerIP();
  initLivePingSimulation();
});

/* Sticky Header styling on scroll */
function initNavbarScroll() {
  const header = document.querySelector('.site-header');
  window.addEventListener('scroll', () => {
    if (window.scrollY > 40) {
      header.classList.add('scrolled');
    } else {
      header.classList.remove('scrolled');
    }
  });
}

/* Ambient canvas particle background */
function initAmbientParticles() {
  const canvas = document.getElementById('ambient-canvas');
  if (!canvas) return;
  const ctx = canvas.getContext('2d');
  
  let width = canvas.width = window.innerWidth;
  let height = canvas.height = window.innerHeight;
  
  window.addEventListener('resize', () => {
    width = canvas.width = window.innerWidth;
    height = canvas.height = window.innerHeight;
  });
  
  const particleCount = 38;
  const particles = [];
  
  for (let i = 0; i < particleCount; i++) {
    particles.push({
      x: Math.random() * width,
      y: Math.random() * height,
      radius: Math.random() * 2 + 0.8,
      speedX: (Math.random() - 0.5) * 0.45,
      speedY: (Math.random() - 0.5) * 0.45,
      color: Math.random() > 0.4 ? 'rgba(0, 229, 153, ' : 'rgba(0, 180, 216, ',
      alpha: Math.random() * 0.4 + 0.1
    });
  }
  
  function draw() {
    ctx.clearRect(0, 0, width, height);
    
    for (let i = 0; i < particleCount; i++) {
      const p = particles[i];
      p.x += p.speedX;
      p.y += p.speedY;
      
      if (p.x < 0) p.x = width;
      if (p.x > width) p.x = 0;
      if (p.y < 0) p.y = height;
      if (p.y > height) p.y = 0;
      
      ctx.beginPath();
      ctx.arc(p.x, p.y, p.radius, 0, Math.PI * 2);
      ctx.fillStyle = p.color + p.alpha + ')';
      ctx.shadowBlur = 10;
      ctx.shadowColor = '#00e599';
      ctx.fill();
    }
    
    requestAnimationFrame(draw);
  }
  
  draw();
}

/* Download tabs switcher (Full Client vs Compact Patch) */
function initDownloadTabs() {
  const tabBtns = document.querySelectorAll('.download-tab-btn');
  const panes = document.querySelectorAll('.download-content-pane');
  
  tabBtns.forEach(btn => {
    btn.addEventListener('click', () => {
      const targetId = btn.getAttribute('data-tab');
      
      tabBtns.forEach(b => b.classList.remove('active'));
      panes.forEach(p => p.classList.remove('active'));
      
      btn.classList.add('active');
      const targetPane = document.getElementById(targetId);
      if (targetPane) targetPane.classList.add('active');
    });
  });
}

/* FAQ Accordion */
function initFaqAccordion() {
  const items = document.querySelectorAll('.faq-item');
  items.forEach(item => {
    const questionBtn = item.querySelector('.faq-question');
    questionBtn.addEventListener('click', () => {
      const isOpen = item.classList.contains('active');
      // close all
      items.forEach(i => i.classList.remove('active'));
      if (!isOpen) {
        item.classList.add('active');
      }
    });
  });
}

/* 1-Click Server IP Copy to Clipboard */
function initCopyServerIP() {
  const copyBtns = document.querySelectorAll('.btn-copy-ip');
  const toast = document.getElementById('toast');
  const toastMsg = document.getElementById('toast-msg');
  let toastTimer = null;
  
  copyBtns.forEach(btn => {
    btn.addEventListener('click', () => {
      const ip = btn.getAttribute('data-ip');
      if (!ip) return;
      
      navigator.clipboard.writeText(ip).then(() => {
        showToast('آدرس سرور با موفقیت کپی شد! در کنسول بازی تایپ کنید: connect ' + ip);
        const originalText = btn.textContent;
        btn.textContent = 'کپی شد!';
        btn.style.backgroundColor = '#00ffb3';
        btn.style.color = '#05140f';
        
        setTimeout(() => {
          btn.textContent = originalText;
          btn.style.backgroundColor = '';
          btn.style.color = '';
        }, 2000);
      }).catch(err => {
        // Fallback
        const textarea = document.createElement('textarea');
        textarea.value = ip;
        document.body.appendChild(textarea);
        textarea.select();
        document.execCommand('copy');
        document.body.removeChild(textarea);
        showToast('آدرس سرور با موفقیت کپی شد: ' + ip);
      });
    });
  });
  
  function showToast(msg) {
    if (!toast) return;
    toastMsg.textContent = msg;
    toast.classList.add('show');
    clearTimeout(toastTimer);
    toastTimer = setTimeout(() => {
      toast.classList.remove('show');
    }, 3500);
  }
}

/* Dynamic Ping Simulation for high immersion */
function initLivePingSimulation() {
  const pingElements = document.querySelectorAll('.server-ping span');
  setInterval(() => {
    pingElements.forEach(el => {
      const base = parseInt(el.getAttribute('data-base-ping') || '15', 10);
      const variance = Math.floor(Math.random() * 5) - 2;
      const current = Math.max(9, base + variance);
      el.textContent = current + 'ms';
    });
  }, 4000);
}
