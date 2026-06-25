#!/usr/bin/env python3
"""Simple PDF generation using wkhtmltopdf or browser print"""
import os
import subprocess
import sys

def check_wkhtmltopdf():
    """Check if wkhtmltopdf is available"""
    try:
        subprocess.run(['wkhtmltopdf', '--version'], 
                      capture_output=True, check=True)
        return True
    except (subprocess.CalledProcessError, FileNotFoundError):
        return False

def generate_pdf_with_wkhtmltopdf(html_path, pdf_path):
    """Generate PDF using wkhtmltopdf"""
    print(f"Generating PDF with wkhtmltopdf...")
    
    # wkhtmltopdf command
    cmd = [
        'wkhtmltopdf',
        '--page-size', 'A4',
        '--margin-top', '20mm',
        '--margin-right', '20mm',
        '--margin-bottom', '20mm',
        '--margin-left', '20mm',
        '--encoding', 'UTF-8',
        '--no-stop-slow-scripts',
        '--disable-smart-shrinking',
        html_path,
        pdf_path
    ]
    
    try:
        result = subprocess.run(cmd, capture_output=True, text=True, check=True)
        print(f"✅ PDF generated successfully: {pdf_path}")
        return True
    except subprocess.CalledProcessError as e:
        print(f"❌ wkhtmltopdf failed: {e}")
        print(f"Error output: {e.stderr}")
        return False

def generate_pdf_with_chromium(html_path, pdf_path):
    """Generate PDF using Chromium headless"""
    print("Generating PDF with Chromium headless...")
    
    # Try common Chromium paths
    chromium_paths = [
        'C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe',
        'C:\\Program Files (x86)\\Google\\Chrome\\Application\\chrome.exe',
        'C:\\Users\\%USERNAME%\\AppData\\Local\\Google\\Chrome\\Application\\chrome.exe'
    ]
    
    for path in chromium_paths:
        expanded_path = os.path.expandvars(path)
        if os.path.exists(expanded_path):
            cmd = [
                expanded_path,
                '--headless',
                '--disable-gpu',
                '--no-sandbox',
                '--disable-setuid-sandbox',
                '--print-to-pdf=' + pdf_path,
                html_path
            ]
            
            try:
                result = subprocess.run(cmd, capture_output=True, text=True, check=True)
                print(f"✅ PDF generated successfully: {pdf_path}")
                return True
            except (subprocess.CalledProcessError, FileNotFoundError) as e:
                continue
    
    print("Chromium not found at common paths")
    return False

def main():
    # Paths
    html_path = "hermes_a1_dossier_v2.0.html"
    pdf_path = "hermes_a1_dossier_v2.0.pdf"
    
    # Check if HTML file exists
    if not os.path.exists(html_path):
        print(f"Error: HTML file not found: {html_path}")
        print("Available files:")
        for file in os.listdir('.'):
            if file.endswith('.html'):
                print(f"  - {file}")
        return 1
    
    # Try wkhtmltopdf first
    if check_wkhtmltopdf():
        if generate_pdf_with_wkhtmltopdf(html_path, pdf_path):
            return 0
    
    # Try Chromium
    if generate_pdf_with_chromium(html_path, pdf_path):
        return 0
    
    # Fallback: Instructions for manual PDF generation
    print("\n❌ Could not generate PDF automatically.")
    print("\n📋 Manual PDF Generation Options:")
    print("1. Browser Print:")
    print("   - Open hermes_a1_dossier_v2.0.html in Chrome/Firefox")
    print("   - Press Ctrl+P (or Cmd+P on Mac)")
    print("   - Select 'Save as PDF' as destination")
    print("   - Adjust settings and save")
    
    print("\n2. Online PDF Generators:")
    print("   - https://html2pdf.com/")
    print("   - https://online-convert.com/")
    print("   - https://pdfcrowd.com/")
    
    print("\n3. Install wkhtmltopdf:")
    print("   - On Ubuntu/Debian: sudo apt-get install wkhtmltopdf")
    print("   - On macOS: brew install wkhtmltopdf")
    print("   - On Windows: Download from https://wkhtmltopdf.org/downloads.html")
    
    return 1

if __name__ == "__main__":
    sys.exit(main())