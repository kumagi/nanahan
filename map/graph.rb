require 'ap'

result = File.open("result.txt", "r"){ |f|
  result = f.each_line.to_a
  result.map!{|n| n.chomp.sub("\t", " ")}

  def cut_target target, work
    cut = []
    state = false
    target.each{ |l|
      if state == true && l.match(/^ .* qps$/)
        cut << l
        next
      end
      if l.match /^#{work}/
        state = true
        next
      end
      state = false
    }
    cut.map{|l| l.sub(/ qps/,"")}.map{|l| l.sub(/^ /,"")}
  end

  def counting target
    result = {}
    target.each{ |n|
      name = n.scan(/(^.*?) *:/)[0][0]
      doing = n.scan(/:(\d*)/)[0][0]
      result[name] ||= {}
      result[name][doing] ||= []
      result[name][doing] << n.scan(/ ([0-9]*$)/)[0][0].to_s
    }
    result
  end


  def get_result target
    def avg_and_var ar
      sum = ar.inject(0.0){|r,i| r+=i.to_i } / ar.size
      avg = ar.inject(0.0){|r,i| r+=(i.to_i-sum)**2 }/ar.size
      { :avg=> sum, :var=> avg }
    end
    counting(target).inject({}){|name, result_array|
      k, v = result_array
      name[k] = v.inject({}){|result, arr|
        nums, value = arr
        result[nums] = avg_and_var(value)
        result
      }
      name
    }
  end

  { :insert => get_result(cut_target(result,"insert")),
    :find =>  get_result(cut_target(result,"find")),
    :erase => get_result(cut_target(result,"erase"))
  }
}

require 'gruff'
def save_graph result,work
  g = Gruff::Line.new "800x600"
  g.theme_37signals
  g.title = "#{work} result"
  target = result[work]
  target.each{ |k,v|
    g.data(k,
           v.map{|k,v| v[:avg]})
  }
  g.y_axis_label = "query / sec"
  g.x_axis_label = "#{work} quantity"

  label = {}
  100.times{ |n|
    next if n % 2 == 1
    label[n] = ((n+1) * 1000000).to_s
  }
  g.labels = label
  g.write("#{work}_graph.png")
end

save_graph result,:insert
save_graph result,:find
save_graph result,:erase
