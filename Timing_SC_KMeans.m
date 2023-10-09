clear all
X_read = readtable('C:\Users\mhashemi\OneDrive - Worcester Polytechnic Institute (wpi.edu)\Desktop\PHD\Paper\Crypto\GT_Updated_MULT_half_256.csv','ReadVariableNames', false);
X2 = table2array(X_read);
size = length(X2(1,:)) - 1;
Bin = X2(:,1);
X3 = X2(2:length(X2()),2:1:size+1)
X_trans = X3';

%     Y = pdist(X_trans(:,3));
%     T = linkage(Y, 'complete');
% dendrogram(T)
% for i = 1:length(X_trans)
%     f1(:,i) = X_trans(1:2:size,i);
%     f2(:,i) = X_trans(2:2:size,i);
% end

%half gate with or
for i = 1:length(X_trans)
    f1(:,i) = X_trans(1:3:size,i);
    f2(:,i) = X_trans(2:3:size,i);
    f3(:,i) = X_trans(3:3:size,i);
end



%%%%%%%%%%%%%%%%%%%%%%%%%%%% mean y=ax+b
for i = 1:length(X_trans)
    a(i) = (max(f1(:,i)) - mean(f1(:,i)))/(max(f2(:,i)) - mean(f2(:,i)));
    b(i) = mean(f1(:,i)) - (a(i) *  mean(f2(:,i)));
    for j = 1:length(f3(:,i))
        middle = f2(j,i);
        f4(j,i) = (a(i) * middle) + b(i);
    end
end
for i = 1:length(X_trans)
    n_n(1:3:size,i) = normalize(f1(:,i),'zscore','std');%f1(:,i);
    n_n(2:3:size,i) = normalize(f4(:,i),'zscore','std');%f2(:,i);
    n_n(3:3:size,i) = normalize(f3(:,i),'zscore','std');%f3(:,i);   
end

% m = 1;
% % for i = length(X_trans)
% %     for j = length(f2)
% %         if(X_trans(3:3:size,i) == f2(:,j))
% %             ind_f2(m) = i; 
% %             m = m + 1;
% %         end
% %     end
% % end
% for i = cast((length(X_trans)/3),"int8")
%     ind_f2(i) = i*3;
% end
% f1 = X_trans;
% f1(:,ind_f2(i))=[];
% % for i = 1:length(X_trans)
% %     n(1:2:5,i) = (X_trans(1:2:5,i)-min(reshape(f1,[64*3,1])))./(max(reshape(f1,[64*3,1]))-min(reshape(f1,[64*3,1])));
% %     n(2:2:6,i) = (X_trans(2:2:6,i)-min(reshape(f2,[64*3,1])))./(max(reshape(f2,[64*3,1]))-min(reshape(f2,[64*3,1])));
% % end


% %normalization
% for i = 1:length(X_trans)
%     n_n(1:3:size,i) = normalize(f1(:,i),'scale','std');
%     n_n(2:3:size,i) = normalize(f2(:,i),'scale','std');
%     n_n(3:3:size,i) = normalize(f3(:,i),'scale','std');
% end

% boxcox
% for i = 1:length(X_trans)
%     [transdat1,lambda1] = boxcox(f1(:,i));
%     [transdat2,lambda2] = boxcox(f2(:,i));
%     [transdat3,lambda3] = boxcox(f3(:,i));
%     n_n(1:3:size,i) = transdat1;
%     n_n(2:3:size,i) = transdat2;
%     n_n(3:3:size,i) = transdat3;
% end
% 
% for i = 1:length(X_trans)
%     [transdat1,lambda1] = boxcox(X_trans(:,i));
%       n_all(1:size,i) = transdat1;
% end

% for i = 1:length(X_trans)
%     n(1+1:2:size-1,i) = (f1(:,i)-min(f1(:,i)))./(max(f1(:,i))-min(f1(:,i)));
%     n(2+1:2:size,i) = (f2(:,i)-min(f2(:,i)))./(max(f2(:,i))-min(f2(:,i)));
% end
% for i = 1:length(X_trans)
%     n_all(1:size,i) = normalize(X_trans(:,i),'norm',1);
% end
% for i = 1:length(X_trans)
%     n_mm_all(1:size,i) = (X_trans(:,i)-min(X_trans(:,i)))./(max(X_trans(:,i))-min(X_trans(:,i)));;
% end
% for i = 1:length(n)
% %     test = reshape(X_trans_3d(i,:,:),[6,2])
% %     Y = pdist(n(:,i));
% %     T = linkage(Y, 'complete');
% % %     idx(i,:) = cluster(T,'cutoff',0.24)';
% % idx(:,i) = cluster(T,'maxclust',2)';
% idx_n_n(:,i) = kmeans(n_n(:,i),2);
% %     idx = reshape(idx,[64,6])
% %     idx = cluster(T,"cutoff",10);
% %     idx1(:,i) = knnsearch(X_trans(:,i),Y);
% end
%     Y = pdist(n(:,4));
%     T = linkage(Y, 'complete');
% dendrogram(T)
% for i = 1:length(X_trans)
%     n(:,i) = (X_trans(:,i)-min(X_trans(:,i)))./(max(X_trans(:,i))-min(X_trans(:,i)));
% end
% Y = pdist(n(:,3));
% T = linkage(Y, 'complete');
% dendrogram(T)
% for i = 1:length(n)
%     idx(:,i) = kmeans(n,2);
% end
% X = reshape(X_trans,[64*6,1])
% for i = 1:length(X)
% %     for j = 1:length(X(1,:))
% % %         if(mod(j,2)==1)
% % %             X_trans_3d(i,j,1) = 1;
% % %         end
% % %         if(mod(j,2)==0)
% % %             X_trans_3d(i,j,1) = 2;
% % %         end
% %         X_trans_3d(i,j,1) = X(i,j);
% %         X_trans_3d(i,j,2) = X(i,j);
% %     end
%    X(i,2) = X(i,1);
% end
%'Options',opts,
opts = statset('Display','final');

for i = 1:length(X_trans)
    n_ind(:,i) = n_n(1:size,i);
    [idx(:,i),C] = kmeans(n_ind(:,i),2,'Distance','sqeuclidean','Replicates',100,'OnlinePhase', 'on','Options',opts);
%     n_ind(:,i) = n_all(1:size,i);
%     [idx(:,i),C] = kmeans(n_ind(:,i),2,'Distance','sqeuclidean','Replicates',100,'OnlinePhase', 'on','Options',opts);
end
binar = zeros(length(X_trans),size);
for i = 1:length(X_trans)
    bb = 1;
    in = Bin(i);
    while in >= 1
        binar(i,bb) = mod(in,2);
        in = in - mod(in,2);
        in = in / 2;
        bb = bb + 1;
    end
end
binar = binar';
zero_clust = 0;
one_clust = 0;
for i = 1:length(X_trans)
    if (X_trans(1,i)<55000)
        zero_clust = idx(1,i);
        if(zero_clust == 1)
            one_clust = 2;
        else
            one_clust = 1;
        end
    else
        one_clust = idx(1,i);
        if(one_clust == 1)
            zero_clust = 2;
        else
            zero_clust = 1;
        end
    end
    for j = 1 :size
        if (binar(j,i) == 0)
            binar_clusters(j,i) = zero_clust;
        else
            binar_clusters(j,i) = one_clust;
        end
    end
end

for i = 1 :length(X_trans)
    correct_clust = 0;
    for j = 1 : size
        if(binar_clusters(j,i) == idx(j,i))
            correct_clust = correct_clust + 1;
        end
    end
    SR(i) = correct_clust/size;
end
SR_avg = mean(SR);
SR_min = min(SR);
% eva = evalclusters(n_ind,'kmeans','Silhouette','klist',[1:4]);
%                 noclusters=eva.OptimalK;
% 
% n_ind = [n_ind,n_ind]
% figure;
% plot(n_ind(idx==1,1),n_ind(idx==1,2),'r.','MarkerSize',12)
% hold on
% plot(n_ind(idx==2,1),n_ind(idx==2,2),'b.','MarkerSize',12)
% plot(C(:,1),C(:,1),'kx',...
%      'MarkerSize',15,'LineWidth',3) 
% legend('Cluster 1','Cluster 2','Centroids',...
%        'Location','NW')
% title 'Cluster Assignments and Centroids'
% hold off